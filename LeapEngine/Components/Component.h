#pragma once

namespace leap
{
	class GameObject;

	class Component
	{
	public:
		Component() = default;
		virtual ~Component() = default;

		Component(const Component& other) = delete;
		Component(Component&& other) = delete;
		Component& operator=(const Component& other) = delete;
		Component& operator=(Component&& other) = delete;

		GameObject* GetGameObject() const { return m_pOwner; }

		void Destroy();
		bool IsMarkedAsDead() const { return m_IsMarkedDead; };

		void SetActive(bool isActive);
		bool IsActive() const;

	protected:
		virtual void Awake() const {};
		virtual void Start() const {};
		virtual void OnEnable() const {};
		virtual void OnDisable() const {};
		virtual void Update() const {};
		virtual void FixedUpdate() const {};
		virtual void LateUpdate() const {};
		virtual void Render() const {};
		virtual void OnGUI() const {};
		virtual void OnDestroy() const {};

	private:
		friend GameObject;

		void SetOwner(GameObject* pOwner);

		void ChangeActiveState();
		void TryCallStart();
		void TryCallEnableAndDisable();

		GameObject* m_pOwner{};

		bool m_NextIsActiveLocal{ true };
		bool m_IsActiveLocal{ false };

		bool m_PrevIsActiveWorld{ false };
		bool m_IsActiveWorld{ false };

		bool m_HasStarted{};
		bool m_IsMarkedDead{};
	};
}