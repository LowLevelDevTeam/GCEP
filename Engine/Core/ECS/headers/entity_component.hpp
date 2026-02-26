#pragma once
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

	struct entityElement
	{
		std::uint8_t version = 0;
		std::uint32_t nextfreeID = 0;
		bool active = false;
	};
	class EntityIDGenerator
	{
	public:
		EntityID generateID();
		void destroyEntity(EntityID id);
		bool isValid(EntityID id);

	private:
		void grow(std::size_t newSize);

		std::vector<entityElement> elements;
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
		[[nodiscard]] static std::uint32_t get()
		{
			static std::uint32_t id = m_nextID++;
			return id;
		}
	private:
		/** @brief Global counter incremented each time a new component type is encountered. */
		static inline std::uint32_t m_nextID = 0;
	};
}


#include <Engine/Core/ECS/detail/entity_component.inl>