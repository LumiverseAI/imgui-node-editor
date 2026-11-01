//------------------------------------------------------------------------------
// Force-included into Dear ImGui, the editor and the tests, so a failed
// assertion is counted by the test runner instead of aborting the run.
//------------------------------------------------------------------------------
# pragma once

void NodeEditorTestAssert(const char* expression, const char* file, int line);

# define IM_ASSERT(_EXPR)    ((_EXPR) ? (void)0 : NodeEditorTestAssert(#_EXPR, __FILE__, __LINE__))
# define CRUDE_ASSERT(_EXPR) IM_ASSERT(_EXPR)

// Test-only counters are shared across translation units; production builds do not
// define these hooks and carry no counters. They prove work was skipped, not only I/O.
struct NodeEditorTestSettingsCounters
{
    unsigned long long Lookups = 0;
    unsigned long long Serializations = 0;
};

inline NodeEditorTestSettingsCounters& NodeEditorTestSettingsWork()
{
    static NodeEditorTestSettingsCounters work;
    return work;
}

# define IMGUI_NODE_EDITOR_TEST_SETTINGS_LOOKUP() (++NodeEditorTestSettingsWork().Lookups)
# define IMGUI_NODE_EDITOR_TEST_SETTINGS_SERIALIZE() (++NodeEditorTestSettingsWork().Serializations)
