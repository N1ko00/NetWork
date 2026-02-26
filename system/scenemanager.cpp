#include	"scenemanager.h"
#include	"SceneClassFactory.h"
#include	<utility>
#include    <iostream>

namespace {
	bool ApplySceneChange(const std::string& sceneName,
		std::unordered_map<std::string, std::unique_ptr<IScene>>& scenes,
		std::string& currentSceneName)
	{
		auto obj = SceneClassFactory::getInstance().create(sceneName);
		if (!obj) {
			std::cerr << "[SceneManager] Scene is not registered: " << sceneName << "\n";
			return false;
		}

		obj->init();
		currentSceneName = sceneName;
		scenes[currentSceneName] = std::move(obj);
		return true;
	}
}

// “o˜^‚³‚ê‚Ä‚¢‚éƒV[ƒ“‚ð‘S‚Ä”jŠü‚·‚é
void SceneManager::Dispose() 
{
	// “o˜^‚³‚ê‚Ä‚¢‚é‚·‚×‚ÄƒV[ƒ“‚ÌI—¹ˆ—
	for (auto& s : m_scenes) 
	{
		s.second->dispose();
	}

	m_scenes.clear();
	m_currentSceneName.clear();
	m_pendingSceneName.clear();
	m_isUpdating = false;
}

void SceneManager::SetCurrentScene(std::string currentscenename) 
{
	if (m_isUpdating) {
		m_pendingSceneName = std::move(currentscenename);
		return;
	}

	ApplySceneChange(currentscenename, m_scenes, m_currentSceneName);
}

void SceneManager::Init()
{
}

void SceneManager::Draw(uint64_t deltatime)
{

	// Œ»Ý‚ÌƒV[ƒ“‚ð•`‰æ
	m_scenes[m_currentSceneName]->draw(deltatime);
}

void SceneManager::Update(uint64_t deltatime)
{
	m_isUpdating = true;
	// Œ»Ý‚ÌƒV[ƒ“‚ðXV
	m_scenes[m_currentSceneName]->update(deltatime);

	if (!m_pendingSceneName.empty()) {
		std::string next = std::move(m_pendingSceneName);
		m_pendingSceneName.clear();
		ApplySceneChange(next, m_scenes, m_currentSceneName);
	}

	m_isUpdating = false;
}