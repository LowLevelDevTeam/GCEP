#pragma once
namespace gcep::panel
{

    class IPanel {
    public:
        virtual ~IPanel() = default;
        virtual void draw() = 0;
    };
}

