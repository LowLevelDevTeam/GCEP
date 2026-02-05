#pragma once

class RHI {
public:
    virtual ~RHI() = default;

protected:
    virtual void initRHI() = 0;
    virtual void drawTriangle() = 0;
};