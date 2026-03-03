#pragma once
#include <Engine/Core/Scene/detail/serializer.inl>
#include  <Engine/Core/Scene/header/scene.hpp>
#include <string>
#include <Engine/Core/Scene/header/json_converter.hpp>
#include <Externals/glaze/glaze.hpp>

namespace gcep::SLS {
#define REFLECT_COMPONENT(ComponentName) \
    friend struct rfl::reflector<ComponentName>; \
    public: static std::string component_type_name() { return #ComponentName; }


    class Serializer
    {
    public:
        bool serialize(std::string path, Scene& scene);
        bool deserialize(std::string path, Scene& scene);
    private:
        template<typename T>
        std::string serializeComponent(const T& component);




    };

}
