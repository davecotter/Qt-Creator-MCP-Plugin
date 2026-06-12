#pragma once

namespace Qt_MCP_Plugin::Constants {

const char ACTION_ID[] = "Qt_MCP_Plugin.Action";
const char MENU_ID[] = "Qt_MCP_Plugin.Menu";

// About action
const char ABOUT_ACTION_ID[] = "Qt_MCP_Plugin.About";

// MCP Command actions
const char BUILD_ACTION_ID[] = "Qt_MCP_Plugin.Build";
const char DEBUG_ACTION_ID[] = "Qt_MCP_Plugin.Debug";
const char STOP_DEBUG_ACTION_ID[] = "Qt_MCP_Plugin.StopDebug";
const char DEBUG_PLAY_PAUSE_ACTION_ID[] = "Qt_MCP_Plugin.DebugPlayPause";
const char GET_DEBUGGED_APP_STATE_ACTION_ID[] = "Qt_MCP_Plugin.GetDebuggedAppState";
const char LIST_PROJECTS_ACTION_ID[] = "Qt_MCP_Plugin.ListProjects";
const char LIST_BUILD_CONFIGS_ACTION_ID[] = "Qt_MCP_Plugin.ListBuildConfigs";
const char QUIT_ACTION_ID[] = "Qt_MCP_Plugin.Quit";
const char GET_CURRENT_PROJECT_ACTION_ID[] = "Qt_MCP_Plugin.GetCurrentProject";
const char GET_CURRENT_BUILD_CONFIG_ACTION_ID[] = "Qt_MCP_Plugin.GetCurrentBuildConfig";
const char RUN_PROJECT_ACTION_ID[] = "Qt_MCP_Plugin.RunProject";
const char CLEAN_PROJECT_ACTION_ID[] = "Qt_MCP_Plugin.CleanProject";
const char LIST_OPEN_FILES_ACTION_ID[] = "Qt_MCP_Plugin.ListOpenFiles";
const char LIST_SESSIONS_ACTION_ID[] = "Qt_MCP_Plugin.ListSessions";
const char GET_CURRENT_SESSION_ACTION_ID[] = "Qt_MCP_Plugin.GetCurrentSession";
const char SAVE_SESSION_ACTION_ID[] = "Qt_MCP_Plugin.SaveSession";
const char LIST_ISSUES_ACTION_ID[] = "Qt_MCP_Plugin.ListIssues";
const char GET_BUILD_DIAGNOSTICS_ACTION_ID[] = "Qt_MCP_Plugin.GetBuildDiagnostics";
const char GET_METHOD_METADATA_ACTION_ID[] = "Qt_MCP_Plugin.GetMethodMetadata";
const char SET_METHOD_METADATA_ACTION_ID[] = "Qt_MCP_Plugin.SetMethodMetadata";

// MCP Protocol Discovery actions
const char MCP_INITIALIZE_ACTION_ID[] = "Qt_MCP_Plugin.MCPInitialize";
const char MCP_TOOLS_LIST_ACTION_ID[] = "Qt_MCP_Plugin.MCPToolsList";

} // namespace Qt_MCP_Plugin::Constants
