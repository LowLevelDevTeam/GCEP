#pragma once

// STL
#include <cstdint>
#include <numeric>
#include <vector>

/**
 * @namespace gcep
 * @brief Namespace for the Gaming Campus Engine Paris.
 */
namespace gcep::ECS
{
	/** @brief Type used to uniquely identify an entity. */
	using EntityID = std::uint32_t;

	/** @brief Constant representing an invalid or non-existent entity. */
	constexpr EntityID INVALID_VALUE = std::numeric_limits<uint32_t>::max();

	/** @brief Type used for internal indexing within component pools. */
	using Index = std::uint32_t;

	const std::uint32_t VALUE_MASK = 0xFFFFFF;
	/**
	 * @struct entityElement
	 * @brief Represents an entity slot in the entity pool.
	 * @details Stores metadata about an entity including its version,
	 * next available ID in the free list, and active status.
	 */
	struct entityElement
	{
		/** @brief Version number used for entity handle validation and reuse detection. */
		std::uint8_t version = 0;
		/** @brief Next free entity ID in the linked list of available slots. */
		std::uint32_t nextfreeID = 0;
		/** @brief Flag indicating whether this entity slot is currently in use. */
		bool active = false;
	};
	/**
	 * @class EntityIDGenerator
	 * @brief Manages the generation and validation of entity IDs.
	 * @details Implements a free-list allocator pattern with versioning to reuse
	 * destroyed entity IDs while preventing ABA problems through version numbers.
	 */
	class EntityIDGenerator
	{
	public:
		/** @brief Generates a new unique entity ID.
		 * @return EntityID The generated or reused entity ID. */
		EntityID generateID();

		/** @brief Marks an entity as destroyed and adds its ID back to the free list.
		 * @param id The entity ID to destroy. */
		void destroyEntity(EntityID id);

		/** @brief Validates whether an entity ID is currently active and valid.
		 * @param id The entity ID to validate.
		 * @return bool True if the entity is valid and active, false otherwise. */
		bool isValid(EntityID id);

		EntityID forceID(EntityID id);

	private:
		/** @brief Expands the entity pool to accommodate more entities.
		 * @param newSize The new capacity for the entity pool. */
		void grow(std::size_t newSize);

		/** @brief Pool of entity metadata elements. */
		std::vector<entityElement> elements;
		/** @brief Index of the first available entity ID in the free list. */
		EntityID nextAvailable = INVALID_VALUE;
	};

	/**
	 * @class ComponentIDGenerator
	 * @brief Static identifier generator for component types.
	 * @details Uses a static member within a template function to assign a unique
	 * ID to each component class at runtime. This ensures O(1) type-to-ID mapping.
	 */
	class ComponentIDGenerator
	{
	public:
		/**
		 * @brief Retrieves the unique ID associated with type T.
		 * @details The ID is generated during the first call for type T,
		 * then cached statically for all subsequent calls.
		 * @tparam T The component type (e.g., Position, Velocity).
		 * @return uint32_t The unique identifier for the component type.
		 */
		template<typename T>
		static std::uint32_t get()
		{
			static std::uint32_t id = nextID(); // initialisé lazily
			return id;
		}

		static std::uint32_t count() { return m_nextID; }

	private:
		static std::uint32_t nextID()
		{
			return m_nextID++;
		}

		/** @brief Global counter incremented each time a new component type is encountered. */
		static inline std::uint32_t m_nextID = 0;
	};
} // namespace gcep::ECS

#include <ECS/detail/entity_component.inl>
