//------------------------------------------------------------------------------
// The view: where navigation takes it, and the grid drawn under it.
//------------------------------------------------------------------------------
# include "harness.h"
# include <cmath>
# include <limits>

namespace {

// Vertices in the color of the grid at zoom 1.
int GridVertices(const ImDrawList* drawList)
{
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(ed::GetStyle().Colors[ed::StyleColor_Grid]);
    int count = 0;
    for (auto& vertex : drawList->VtxBuffer)
        if (vertex.col == color)
            ++count;
    return count;
}

} // namespace

TEST_CASE(GridAtTinyZoom, "view.grid_at_tiny_zoom_is_bounded", "CORE-10", test::TestFlags_None)
{
    auto content = [](int)
    {
        ed::BeginNode(1);
        ImGui::TextUnformatted("Node");
        ed::EndNode();
    };

    {
        test::Storage storage;
        test::Session session(&storage);
        session.Frame(content);
        session.Frame(content);
        const int gridVertices = session.Use([&] { return GridVertices(session.DrawList); });
        test::Check(gridVertices > 0, "grid drawn at zoom 1: %d vertices", gridVertices);
    }

    // Zoom levels down to 1e-6, where the view is 10^9 units across.
    test::Storage storage;
    storage.Global = R"({"view":{"scroll":{"x":0,"y":0},"zoom":0.000001}})";
    auto config = test::MakeConfig(&storage);
    config.CustomZoomLevels.push_back(1e-6f);
    config.CustomZoomLevels.push_back(1.0f);
    test::Session session(config);
    session.Settings = &storage;
    for (int i = 0; i < 3; ++i)
        session.Frame(content);
    const float zoom = session.Use([] { return test::CurrentZoom(); });
    test::Check(zoom < 1e-5f, "view zoomed out to %g", zoom);
}

TEST_CASE(MinimapNavigation, "view.navigate_to_point_preserves_zoom_selection_and_nodes", "UIKIT-201", test::TestFlags_None)
{
    auto config = test::MakeConfig(nullptr);
    config.EnableSettingsPersistence = false;
    test::Session session(config);
    session.DefaultContent = [](int frame)
    {
        if (frame == 0)
            ed::SetNodePosition(1, ImVec2(100, 200));
        ed::BeginNode(1);
        ImGui::TextUnformatted("Node");
        ed::EndNode();
    };
    session.Run(3);
    session.Use([] { ed::SelectNode(1); ed::NavigateToPoint(ImVec2(5000, -3000), 0); });
    session.Run(3);
    session.Use([] { ed::NavigateToPoint(ImVec2(std::numeric_limits<float>::quiet_NaN(), 0), 0); });
    session.Run(2);
    session.Use([]
    {
        const auto origin = ed::CanvasToScreen(ImVec2(0, 0));
        const auto center = ed::ScreenToCanvas(origin + ed::GetScreenSize() * .5f);
        const auto position = ed::GetNodePosition(1);
        test::Check(std::fabs(center.x - 5000) < .01f && std::fabs(center.y + 3000) < .01f,
            "view centered on the minimap point: (%g, %g)", center.x, center.y);
        test::Check(test::CurrentZoom() == 1, "zoom stays unchanged");
        test::Check(ed::IsNodeSelected(1) && position.x == 100 && position.y == 200, "selection and node geometry stay unchanged");
    });
}
