#pragma once
#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include "Engine/Core/Maths/vector3.hpp"

#include <atomic>
#include <vector>
#include <string>

namespace gcep
{
    namespace jolt
    {
        // Un vertex de debug : position 3D + couleur RGBA compactée en 32 bits.
        // uint32_t couleur car Jolt fournit Color::GetUInt32() directement,
        // et Vulkan peut dépaqueter R8G8B8A8_UNORM automatiquement dans le shader.
        struct DebugVertex
        {
            Vector3<float> position;
            uint32_t       color;
        };

        // Un texte 3D à afficher : position monde + texte + couleur + hauteur.
        // Accumulé ici (Engine), consommé par l'éditeur via getTextEntries() pour ImGui.
        struct TextEntry
        {
            Vector3<float> worldPos;
            std::string    text;
            uint32_t       color;
            float          height;
        };

        class PhysicsDebugRenderer final : public JPH::DebugRenderer
        {
        public:
            PhysicsDebugRenderer() { Initialize(); }

            // Appelé au début de chaque frame : vide les 3 accumulateurs.
            // Doit être appelé AVANT que Jolt ne dessine les bodies.
            void beginFrame()
            {
                m_linesVertices.clear();
                m_triVertices.clear();
                m_textEntries.clear();
            }

            // Accesseurs pour le VulkanRHI (lit ces données pour les uploader en GPU)
            const std::vector<DebugVertex>& getLineVertices() const { return m_linesVertices; }
            const std::vector<DebugVertex>& getTriVertices()  const { return m_triVertices;   }

            // Accesseur pour l'éditeur (affichage via ImGui)
            const std::vector<TextEntry>&   getTextEntries()  const { return m_textEntries;   }

            // --- Interface Jolt (méthodes pures virtuelles à implémenter) ---
            void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
            void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;
            void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view &inString, JPH::ColorArg inColor, float inHeight) override;
            void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox &inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef &inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode) override;
            Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) override;
            Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const JPH::uint32 *inIndices, int inIndexCount) override;

        private:
            // Cache d'une shape physique. Créé UNE SEULE FOIS par shape via CreateTriangleBatch,
            // puis passé à DrawGeometry chaque frame via GeometryRef.
            // Hérite de RefTargetVirtual pour que Jolt gère la durée de vie via Ref<>.
            class BatchImpl : public JPH::RefTargetVirtual
            {
            public:
                void AddRef()  override { ++m_refCount; }
                void Release() override { if (--m_refCount == 0) delete this; }

                JPH::Array<Triangle> m_triangles; // triangles de la shape, en local space
            private:
                std::atomic<uint32_t> m_refCount{0};
            };

            // Accumulateurs par frame (vidés dans beginFrame, lus par VulkanRHI dans flushPhysicsDebugData)
            std::vector<DebugVertex> m_linesVertices; // 2 vertices par ligne
            std::vector<DebugVertex> m_triVertices;   // 3 vertices par triangle
            std::vector<TextEntry>   m_textEntries;   // textes 3D pour l'éditeur
        };
    }
} // gcep

#endif