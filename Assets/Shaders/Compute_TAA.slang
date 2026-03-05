[[vk::binding(0, 0)]] Texture2D<float4>   currentColor;
[[vk::binding(1, 0)]] Texture2D<float4>   historyColor;
[[vk::binding(2, 0)]] Texture2D<float2>   motionVectors;
[[vk::binding(3, 0)]] RWTexture2D<float4> resolvedOutput;
[[vk::binding(4, 0)]] SamplerState linearSampler;

struct TAAPushConstants
{
    float2 texelSize;
    float  blendAlpha;      // weight of current frame
    float  varianceGamma;   // AABB expansion multiplier
};

[[vk::push_constant]]
TAAPushConstants pc;

/// Convert linear RGB to YCoCg.
float3 rgbToYCoCg(float3 rgb)
{
    float y  =  0.25f * rgb.r + 0.5f * rgb.g + 0.25f * rgb.b;
    float co =  0.5f  * rgb.r                - 0.5f  * rgb.b;
    float cg = -0.25f * rgb.r + 0.5f * rgb.g - 0.25f * rgb.b;
    return float3(y, co, cg);
}

/// Convert YCoCg back to linear RGB.
float3 yCoCgToRgb(float3 ycocg)
{
    float y  = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;

    float r = y + co - cg;
    float g = y      + cg;
    float b = y - co - cg;
    return float3(r, g, b);
}

// Variance-clip history to neighborhood AABB (YCoCg space)
float3 clipToAABB(float3 history, float3 aabbMin, float3 aabbMax)
{
    float3 center  = 0.5f * (aabbMax + aabbMin);
    float3 extents = 0.5f * (aabbMax - aabbMin);

    // Move history relative to center, then scale back into [-extents, +extents]
    float3 delta = history - center;
    float3 units = delta / max(extents, 1e-5f);

    float  maxUnit = max(abs(units.x), max(abs(units.y), abs(units.z)));

    if (maxUnit > 1.0f)
        return center + delta / maxUnit;
    else
        return history;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compMain(uint3 dispatchID : SV_DispatchThreadID)
{
    const int2 pixelCoord = int2(dispatchID.xy);

    float3 m1 = 0.0f; // Sum of samples
    float3 m2 = 0.0f; // Sum of samples squared

    float3 currentRGB = 0.0f;

    float2 bestMotion = motionVectors.Load(int3(pixelCoord, 0)).rg;
    float maxMag = dot(bestMotion, bestMotion);

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            int2   sampleCoord = pixelCoord + int2(dx, dy);
            float3 sample3     = currentColor.Load(int3(sampleCoord, 0)).rgb;
            float3 ycocg       = rgbToYCoCg(sample3);

            m1 += ycocg;
            m2 += ycocg * ycocg;

            if (dx == 0 && dy == 0)
                currentRGB = sample3;

            float2 m = motionVectors.Load(int3(pixelCoord + int2(dx, dy), 0)).rg;
            float mag = dot(m, m);
            if (mag > maxMag)
            {
                bestMotion = m;
                maxMag = mag;
            }
        }
    }

    // Compute Mean and Standard Deviation
    const float N = 9.0f;
    float3 mean = m1 / N;
    float3 stddev = sqrt(max(0.0f, (m2 / N) - (mean * mean)));

    // The more variance, the more ghosting, have fun!
    float3 aabbMin = mean - pc.varianceGamma * stddev;
    float3 aabbMax = mean + pc.varianceGamma * stddev;

    // Reproject: read motion vector, compute history UV
    float2 currentUV  = (float2(pixelCoord) + 0.5f) * pc.texelSize;
    float2 historyUV  = currentUV - bestMotion;

    // Sample history with bilinear filtering
    float2 clampedHistoryUV = clamp(historyUV, 0.0f, 1.0f);
    float3 historyRGB  = historyColor.SampleLevel(linearSampler, clampedHistoryUV, 0.0f).rgb;
    float3 historyYCoCg = rgbToYCoCg(historyRGB);

    // Variance-clip history to neighborhood AABB
    float3 clippedYCoCg = clipToAABB(historyYCoCg, aabbMin, aabbMax);
    float3 clippedRGB   = yCoCgToRgb(clippedYCoCg);

    // Reject history entirely when the reprojected UV is out of screen bounds
    float  blendAlpha = pc.blendAlpha;
    if (any(historyUV < 0.0f) || any(historyUV > 1.0f))
        blendAlpha = 1.0f; // if no valid history -> use current frame

    // Blend current and clamped history
    float3 resolved = lerp(clippedRGB, currentRGB, blendAlpha);

    // Write resolved output
    resolvedOutput[pixelCoord] = float4(resolved, 1.0f);
}
