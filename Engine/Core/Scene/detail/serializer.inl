#pragma once
#include <Engine/Core/Scene/header/serializer.hpp>


namespace gcep::SLS
{
    inline bool Serializer::serialize(std::string path, Scene& scene)
    {
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
        rapidjson::Value sceneName;
        sceneName.SetString(scene.getName().c_str(), allocator);
        doc.AddMember("SceneName", sceneName, allocator);

        rapidjson::Value allPools(rapidjson::kObjectType);
        for (const auto& pool : scene.getRegistry().getPools())
        {
            if (!pool) continue;
            rapidjson::Value componentArray(rapidjson::kArrayType);
            for (ECS::EntityID entityID : pool->getEntities())
            {
                rapidjson::Value component(rapidjson::kObjectType);

                component.AddMember("id",entityID,allocator);
                component.AddMember("");

            }
        }
    }

    inline bool Serializer::deserialize(std::string path,Scene& scene)
    {
        for (auto& pool : scene.getRegistry().getPools())
        {

        }
    }

   /** inline bool Serializer::serializeComponent(const T& component, rapidjson::Document::AllocatorType& allocator)
    {
        if constexpr (std::is_same_v<T, Vector2<float>> || std::is_same_v<T, Vector2<int>>) {
            return to_json_vec2(component, allocator);
        }
        // Si c'est un Vector3
        else if constexpr (std::is_same_v<T, Vector3<float>> || std::is_same_v<T, Vector3<int>>) {
            return to_json_vec3(component, allocator);
        }
        // Si c'est un Quaternion
        else if constexpr (std::is_same_v<T, Quaternion>) {
            return to_json_quaternion(component, allocator);
        }
        // Si c'est une Matrice 4x4
        else if constexpr (std::is_same_v<T, Mat4<float>>) {
            return to_json_Mat4(component, allocator);
        }

    }*/


}


