#include "SceneManager.h"

#include <sstream>
#include <stdexcept>

#include "Scene.h"

void leap::SceneManager::AddScene(const std::string& name, const std::function<void(Scene&)>& load)
{
	m_Scenes.emplace_back(SceneData{ name, load });
}

void leap::SceneManager::LoadScene(unsigned index)
{
	m_LoadScene = static_cast<int>(index);
}

void leap::SceneManager::LoadScene(const std::string& name)
{
	if (
		const auto it = std::ranges::find_if(m_Scenes, [&](const auto& scene)
			{
				return scene.name == name;
			}); it != m_Scenes.end()
		)
	{
		m_LoadScene = static_cast<int>(std::distance(m_Scenes.begin(), it));
		return;
	}

	std::stringstream ss{};
	ss << "No scene found with name: " << name;
	throw std::exception{ss.str().c_str()};
}

void leap::SceneManager::OnFrameStart()
{
	if (m_LoadScene >= 0) { LoadInternalScene(); }
	m_Scene->OnFrameStart();
}

void leap::SceneManager::FixedUpdate() const
{
	m_Scene->FixedUpdate();
}

void leap::SceneManager::Update() const
{
	m_Scene->Update();
}

void leap::SceneManager::LateUpdate() const
{
	m_Scene->LateUpdate();
}

void leap::SceneManager::Render() const
{
	m_Scene->Render();
}

void leap::SceneManager::OnGUI() const
{
	m_Scene->OnGUI();
}

void leap::SceneManager::OnFrameEnd() const
{
	m_Scene->OnFrameEnd();
}

void leap::SceneManager::LoadInternalScene()
{
	if (static_cast<unsigned>(m_LoadScene) >= m_Scenes.size())
	{
		throw std::runtime_error("Tried to load scene that isn't known to the scenemanager");
		return;
	}
	const auto& sceneData = m_Scenes[m_LoadScene];
	m_LoadScene = -1;
	m_Scene = std::make_unique<Scene>(sceneData.name);
	sceneData.load(*m_Scene);
}
