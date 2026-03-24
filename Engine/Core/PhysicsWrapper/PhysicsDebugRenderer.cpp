#include "PhysicsDebugRenderer.hpp"
#ifdef JPH_DEBUG_RENDERER

namespace gcep
{
    namespace jolt
    {
        // --- DrawLine ---
        // Jolt appelle cette méthode pour chaque segment de debug (AABBs, velocités, contraintes...).
        // On crée 2 vertices (début + fin) et on les ajoute à l'accumulateur de lignes.
        void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
        {
            uint32_t color = inColor.GetUInt32();
            m_linesVertices.push_back({ {(float)inFrom.GetX(), (float)inFrom.GetY(), (float)inFrom.GetZ()}, color });
            m_linesVertices.push_back({ {(float)inTo.GetX(),   (float)inTo.GetY(),   (float)inTo.GetZ()},   color });
        }

        // --- DrawTriangle ---
        // Jolt appelle cette méthode pour des triangles individuels (contacts, triggers...).
        // On crée 3 vertices et on les ajoute à l'accumulateur de triangles.
        void PhysicsDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3,
            JPH::ColorArg inColor, ECastShadow inCastShadow)
        {
            uint32_t color = inColor.GetUInt32();
            m_triVertices.push_back({ {(float)inV1.GetX(), (float)inV1.GetY(), (float)inV1.GetZ()}, color });
            m_triVertices.push_back({ {(float)inV2.GetX(), (float)inV2.GetY(), (float)inV2.GetZ()}, color });
            m_triVertices.push_back({ {(float)inV3.GetX(), (float)inV3.GetY(), (float)inV3.GetZ()}, color });
        }

        // --- DrawText3D ---
        // Jolt appelle cette méthode pour afficher du texte flottant dans le monde (IDs, valeurs...).
        // On stocke les données côté moteur, l'éditeur les lira via getTextEntries() pour ImGui.
        void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view &inString,
            JPH::ColorArg inColor, float inHeight)
        {
            m_textEntries.push_back({
                {(float)inPosition.GetX(), (float)inPosition.GetY(), (float)inPosition.GetZ()},
                std::string(inString),
                inColor.GetUInt32(),
                inHeight
            });
        }

        // --- CreateTriangleBatch (version Triangle[]) ---
        // Jolt appelle cette méthode UNE SEULE FOIS par shape pour créer le cache GPU.
        // On stocke les triangles dans un BatchImpl. Jolt le garde en vie via Ref<>.
        JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(const Triangle *inTriangles,
            int inTriangleCount)
        {
            BatchImpl* batch = new BatchImpl();
            if (inTriangles != nullptr && inTriangleCount > 0)
                batch->m_triangles.assign(inTriangles, inTriangles + inTriangleCount);
            return batch; // converti automatiquement en Batch (= Ref<RefTargetVirtual>)
        }

        // --- CreateTriangleBatch (version indexée) ---
        // Même chose mais avec un tableau de vertices + indices séparés.
        // On déroule les indices pour obtenir une liste plate de triangles.
        JPH::DebugRenderer::Batch PhysicsDebugRenderer::CreateTriangleBatch(const Vertex *inVertices, int inVertexCount,
            const JPH::uint32 *inIndices, int inIndexCount)
        {
            BatchImpl* batch = new BatchImpl();
            if (inVertices != nullptr && inIndices != nullptr && inIndexCount > 0)
            {
                batch->m_triangles.reserve(inIndexCount / 3);
                for (int i = 0; i < inIndexCount; i += 3)
                {
                    Triangle t;
                    t.mV[0] = inVertices[inIndices[i]];
                    t.mV[1] = inVertices[inIndices[i + 1]];
                    t.mV[2] = inVertices[inIndices[i + 2]];
                    batch->m_triangles.push_back(t);
                }
            }
            return batch;
        }

        // --- DrawGeometry ---
        // Jolt appelle cette méthode CHAQUE FRAME pour chaque body visible.
        // On reçoit le BatchImpl créé dans CreateTriangleBatch + la model matrix du body.
        // On CPU-transforme les vertices du local space vers le world space, puis on les
        // ajoute à l'accumulateur approprié (triangles ou lignes selon le draw mode).
        void PhysicsDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds,
            float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode,
            ECastShadow inCastShadow, EDrawMode inDrawMode)
        {
            // Utilise le premier LOD (le plus détaillé)
            if (inGeometry->mLODs.empty()) return;
            const BatchImpl* batch = static_cast<const BatchImpl*>(inGeometry->mLODs[0].mTriangleBatch.GetPtr());
            if (batch == nullptr) return;

            const bool wireframe = (inDrawMode == EDrawMode::Wireframe);

            for (const Triangle& tri : batch->m_triangles)
            {
                DebugVertex verts[3];
                for (int i = 0; i < 3; ++i)
                {
                    // Transforme la position du local space vers le world space.
                    // Multiply3x4 = multiplication 3x4 (traite le vertex comme un point, w=1, applique translation).
                    JPH::Vec3 localPos(tri.mV[i].mPosition.x, tri.mV[i].mPosition.y, tri.mV[i].mPosition.z);
                    JPH::Vec3 worldPos = inModelMatrix * localPos;

                    // Multiplie la couleur du vertex par la couleur du model (tint).
                    // Division par 255 pour rester dans [0, 255] après multiplication.
                    JPH::Color vc = tri.mV[i].mColor;
                    JPH::Color mc = inModelColor;
                    uint32_t finalColor = JPH::Color(
                        static_cast<uint8_t>(static_cast<int>(vc.r) * mc.r / 255),
                        static_cast<uint8_t>(static_cast<int>(vc.g) * mc.g / 255),
                        static_cast<uint8_t>(static_cast<int>(vc.b) * mc.b / 255),
                        static_cast<uint8_t>(static_cast<int>(vc.a) * mc.a / 255)
                    ).GetUInt32();

                    verts[i] = { {(float)worldPos.GetX(), (float)worldPos.GetY(), (float)worldPos.GetZ()}, finalColor };
                }

                if (wireframe)
                {
                    // Wireframe : 3 arêtes = 6 vertices de lignes
                    m_linesVertices.push_back(verts[0]); m_linesVertices.push_back(verts[1]);
                    m_linesVertices.push_back(verts[1]); m_linesVertices.push_back(verts[2]);
                    m_linesVertices.push_back(verts[2]); m_linesVertices.push_back(verts[0]);
                }
                else
                {
                    // Solid : 3 vertices par triangle
                    m_triVertices.push_back(verts[0]);
                    m_triVertices.push_back(verts[1]);
                    m_triVertices.push_back(verts[2]);
                }
            }
        }
    } // jolt
} // gcep

#endif