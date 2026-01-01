#include <Foundation/Application/Application.h>
#include <Foundation/Logging/ConsoleWriter.h>
#include <Foundation/Logging/VisualStudioWriter.h>
#include <Core/Input/InputManager.h>
#include <GameEngine/GameApplication/GameApplication.h>
#include <RendererCore/Lights/AmbientLightComponent.h>
#include <RendererCore/Lights/DirectionalLightComponent.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <GameEngine/DearImgui/DearImgui.h>


using HelloWorldGameStateBase = ezGameState;

class HelloWorldGameState : public HelloWorldGameStateBase
{
    EZ_ADD_DYNAMIC_REFLECTION(HelloWorldGameState, HelloWorldGameStateBase);

public:
    HelloWorldGameState() = default;
    ~HelloWorldGameState() = default;


protected:
    ezUniquePtr<ezWorld> m_pWorld;

private:
    virtual void OnActivation(ezWorld* pWorld, ezStringView sStartPosition, const ezTransform& startPositionOffset) override
    {
        EZ_LOG_BLOCK("HelloWorldGameState::Activate");


        SUPER::OnActivation(pWorld, sStartPosition, startPositionOffset);

        EZ_DEFAULT_NEW(ezImgui);

        CreateWorld();

    }

    void CreateWorld()
    {


        ezWorldDesc desc("Asteroids - World");
        m_pWorld = EZ_DEFAULT_NEW(ezWorld, desc);
        m_pMainWorld = m_pWorld.Borrow();
        auto marker = m_pWorld->GetWriteMarker();
        marker.Lock();
        // Lights
        {
            ezGameObjectDesc obj;
            obj.m_sName.Assign("DirLight");
            obj.m_LocalRotation = ezQuat::MakeFromAxisAndAngle(ezVec3(0.0f, 1.0f, 0.0f), -ezAngle::MakeFromDegree(120.0f));

            ezGameObject* pObj;
            m_pWorld->CreateObject(obj, pObj);

            // point and spot lights won't work with the orthographic camera
            ezDirectionalLightComponent* pDirLight;
            ezDirectionalLightComponent::CreateComponent(pObj, pDirLight);

            ezAmbientLightComponent* pAmbLight;
            ezAmbientLightComponent::CreateComponent(pObj, pAmbLight);
        }


        ChangeMainWorld(m_pWorld.Borrow(), {}, ezTransform::MakeIdentity());

        marker.Unlock();
    }
    void AfterWorldUpdate()
    {
        SUPER::AfterWorldUpdate();

        //// BEGIN-DOCS-CODE-SNIPPET: cvar-2
        //if (cvar_DebugDisplay)
        //{
            ezDebugRenderer::DrawLineSphere(m_pMainWorld, ezBoundingSphere::MakeFromCenterAndRadius(ezVec3::MakeZero(), 1.0f), ezColor::Orange);
        //}
        //// END-DOCS-CODE-SNIPPET

        ezDebugRenderer::Draw2DText(m_pMainWorld, "Press 'O' to spawn objects", ezVec2I32(10, 10), ezColor::White);
        ezDebugRenderer::Draw2DText(m_pMainWorld, "Press 'P' to remove objects", ezVec2I32(10, 30), ezColor::White);
    }
    void BeforeWorldUpdate()
    {
        EZ_LOCK(m_pMainWorld->GetWriteMarker());

#ifdef BUILDSYSTEM_ENABLE_IMGUI_SUPPORT
        if (ezImgui::GetSingleton() != nullptr)
        {
            static bool stats = false;
            static bool window = true;
            static float color[3];
            static float slider = 0.5f;

            // BEGIN-DOCS-CODE-SNIPPET: imgui-activate
            ezImgui::GetSingleton()->SetCurrentContextForView(m_hMainView);
            // END-DOCS-CODE-SNIPPET

            ezImgui::GetSingleton()->SetPassInputToImgui(false); // reset this state, to deactivate input processing as long as SampleGameState::ProcessInput() isn't called again

            // BEGIN-DOCS-CODE-SNIPPET: imgui-panel
            ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_FirstUseEver);
            ImGui::Begin("Imgui Window", &window);
            ImGui::Text("Hello World!");
            ImGui::SliderFloat("Slider", &slider, 0.0f, 1.0f);
            ImGui::ColorEdit3("Color", color);


            if (ImGui::Button("Toggle Stats"))
            {
                stats = !stats;
            }

            if (stats)
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            }

            ImGui::End();
            // END-DOCS-CODE-SNIPPET
        }
#endif
    }
};

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(HelloWorldGameState, 1, ezRTTIDefaultAllocator<HelloWorldGameState>)
EZ_END_DYNAMIC_REFLECTED_TYPE;




class ezHelloWorldApp : public ezGameApplication
{
public:
    ezHelloWorldApp() : ezGameApplication("ezHelloWorld", nullptr)
    {
        ezFileSystem::SetSdkRootDirectory("C:\\Users\\casht\\repos\\ezEngineSubModuleExample\\ezEngine");

    }
    std::unique_ptr<ezWorld> world;
    virtual void AfterCoreSystemsStartup() override
    {
        ezGlobalLog::AddLogWriter(ezLogWriter::Console::LogMessageHandler);
        ezGlobalLog::AddLogWriter(ezLogWriter::VisualStudio::LogMessageHandler);



        ExecuteInitFunctions();

        ezStartup::StartupHighLevelSystems();


        // we need a game state to do anything
        // if no custom game state is available, ezFallbackGameState will be used
        // the game state is also responsible for either creating a world, or loading it
        // the ezFallbackGameState inspects the command line to figure out which scene to load
        ActivateGameState(nullptr, {}, ezTransform::MakeIdentity());


        return;
    }

    virtual void BeforeCoreSystemsShutdown() override
    {
        // prevent further output during shutdown
        ezGlobalLog::RemoveLogWriter(ezLogWriter::Console::LogMessageHandler);
        ezGlobalLog::RemoveLogWriter(ezLogWriter::VisualStudio::LogMessageHandler);



    }
    ezResult BeforeCoreSystemsStartup()
    {
        ezStartup::AddApplicationTag("game");

        EZ_SUCCEED_OR_RETURN(SUPER::BeforeCoreSystemsStartup());


        DetermineProjectPath();

        return EZ_SUCCESS;
    }


    ezResult TryProjectFolder(ezStringView sPath)
    {
        ezStringBuilder sProjDir = sPath;
        sProjDir.MakeCleanPath();

        ezStringBuilder sProjFile;
        sProjFile.SetPath(sProjDir, "ezProject");

        if (sProjFile.IsAbsolutePath() && ezOSFile::ExistsFile(sProjFile))
        {
            m_sAppProjectPath = sProjDir;
            return EZ_SUCCESS;
        }

        return EZ_FAILURE;
    }

    void DetermineProjectPath()
    {
        // IMPORTANT!
        //
        // The project path has to be set for the ezGameApplication to know where the main 'project' data directory is.
        // Without it, nothing will work (the game plugin won't be loaded etc).
        //
        // The path can be relative to the '>SDK' directory (the root folder where EZ is located).
        // It may also be absolute (though this isn't portable across machines).
        // Or it can be relative to ezOSFile::GetApplicationDirectory() (where the Game.exe is).
        //
        // If your project is inside the EZ directory, use a relative path from there.
        // If it is somewhere outside, you either need to use an absolute path or some other way to locate it.
        //
        // Note that in a final exported build the project folder is always merged with the EZ data folders into one package.

        // this path works for exported projects, because during export the project folder is always copied there
        ezStringBuilder sProjDir;
        if (ezFileSystem::ResolveSpecialDirectory(">sdk/Data/project", sProjDir).Succeeded())
        {
            if (TryProjectFolder(sProjDir).Succeeded())
                return;
        }

#ifdef GAME_PROJECT_FOLDER
        // this absolute path will only work on the machine where the game is compiled,
        // but it works for projects that are located outside the ezEngine folder
        if (TryProjectFolder(EZ_PP_STRINGIFY(GAME_PROJECT_FOLDER)).Succeeded())
            return;
#endif

        // in other cases, try this relative path
        m_sAppProjectPath = "Data/Samples/Asteroids";
    }

    ezUniquePtr<ezGameStateBase> CreateGameState()
    {
        return HelloWorldGameState::GetStaticRTTI()->GetAllocator()->Allocate<HelloWorldGameState>();
    }


    void Run_InputUpdate()
    {
        SUPER::Run_InputUpdate();

        if (auto pGameState = GetActiveGameState())
        {
            // pass through the closing of the application
            if (pGameState->WasQuitRequested())
            {
                QuitApplication();
            }
        }
    }
};

EZ_APPLICATION_ENTRY_POINT(ezHelloWorldApp);
