#pragma once

// Externals
#include <glm/fwd.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace gcep
{
    /**
     * @namespace Layers
     * @brief Defines object layers used for physics collision filtering.
     */
    namespace Layers
    {
        /// Non-moving/static objects layer.
        static constexpr JPH::ObjectLayer NON_MOVING = 0;

        /// Moving/dynamic objects layer.
        static constexpr JPH::ObjectLayer MOVING = 1;

        /// Total number of object layers.
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    /**
     * @namespace BroadPhaseLayers
     * @brief Defines broadphase layers for Jolt physics.
     */
    namespace BroadPhaseLayers
    {
        /// Non-moving/static objects broadphase layer.
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);

        /// Moving/dynamic objects broadphase layer.
        static constexpr JPH::BroadPhaseLayer MOVING(1);

        /// Total number of broadphase layers.
        static constexpr glm::uint NUM_LAYERS = 2;
    };

    /**
     * @class BPlayerInterfaceImpl
     * @brief Implementation of Jolt's BroadPhaseLayerInterface for mapping object layers to broadphase layers.
     *
     * This class defines how object layers (like moving or non-moving) are mapped
     * to broadphase layers for collision detection.
     */
    class BPlayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        /// Constructor: initializes the object-to-broadphase layer mapping.
        BPlayerInterfaceImpl();

        /**
         * @brief Returns the number of broadphase layers.
         * @return Number of broadphase layers.
         */
        [[nodiscard]] glm::uint getNumBroadPhaseLayers() const override;

        /**
         * @brief Maps an object layer to a broadphase layer.
         * @param layer The object layer.
         * @return Corresponding broadphase layer.
         */
        [[nodiscard]] JPH::BroadPhaseLayer getBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        /**
         * @brief Returns a human-readable name for a broadphase layer (debugging purposes).
         * @param inLayer The broadphase layer.
         * @return Name of the layer as a C-string.
         */
        [[nodiscard]] const char* getBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        /// Internal mapping from object layers to broadphase layers.
        JPH::BroadPhaseLayer m_ObjectToBroadPhase[Layers::NUM_LAYERS];
    };
} // namespace gcep
