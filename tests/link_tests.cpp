//------------------------------------------------------------------------------
// What the editor draws, culls and hit tests of a link.
//------------------------------------------------------------------------------
# include "harness.h"
# include <cstdio>
# include <limits>

namespace {

const ImU32 c_LinkColor = IM_COL32(255, 0, 0, 255);

// Link 100 between pins whose pivots are points in canvas space; their nodes are
// out of the way.
struct LinkScene
{
    ImVec2 Start;
    ImVec2 End;
    ImVec2 StartDirection = ImVec2(1.0f, 0.0f);
    ImVec2 EndDirection   = ImVec2(-1.0f, 0.0f);
    float  Thickness      = 1.0f;
    bool   Outlined       = false;
    ImVec4 Color          = ImColor(c_LinkColor);
    ImVec4 OutlineColor   = ImColor(IM_COL32(0, 255, 0, 255));
    float  OutlineThickness = 3.0f;

    void Submit(int frame) const
    {
        const ImVec2 pivots[2] = { Start, End };
        ed::PushStyleVar(ed::StyleVar_SourceDirection, StartDirection);
        ed::PushStyleVar(ed::StyleVar_TargetDirection, EndDirection);
        for (int n = 0; n < 2; ++n)
        {
            if (frame == 0)
                ed::SetNodePosition(n + 1, ImVec2(n * 300.0f, 2000.0f));
            ed::BeginNode(n + 1);
            ed::BeginPin(11 + n, n == 0 ? ed::PinKind::Output : ed::PinKind::Input);
            ImGui::TextUnformatted("Pin");
            ed::PinPivotRect(pivots[n], pivots[n]);
            ed::EndPin();
            ed::EndNode();
        }
        ed::PopStyleVar(2);
        if (Outlined)
            ed::LinkWithOutline(100, 11, 12, Color, OutlineColor, Thickness, OutlineThickness);
        else
            ed::Link(100, 11, 12, Color, Thickness);
    }
};

int CountLinkVertices(const test::Session& session)
{
    int count = 0;
    for (auto color : test::Capture(session.DrawList).Colors)
        if (color == c_LinkColor)
            ++count;
    return count;
}

} // namespace

TEST_CASE(ThickLinkAtEdge, "links.thick_link_partly_on_screen_is_drawn", "CORE-15", test::TestFlags_None)
{
    // A link 10 thick along the editor's top edge, its centerline 2 above it.
    test::Storage storage;
    test::Session session(&storage);
    ImVec2 editorOrigin;
    session.BeforeEditor = [&] { editorOrigin = ImGui::GetCursorScreenPos(); };
    LinkScene scene;
    scene.Thickness = 10.0f;
    session.DefaultContent = [&scene](int frame) { scene.Submit(frame); };
    session.Run(2);

    const auto top = session.Use([&] { return ed::ScreenToCanvas(editorOrigin).y; });
    scene.Start = ImVec2(100.0f, top - 2.0f);
    scene.End   = ImVec2(600.0f, top - 2.0f);
    session.Run(2);

    const auto vertices = CountLinkVertices(session);
    test::Check(vertices > 0, "the 3 of the link on screen are drawn: %d vertices", vertices);
}

TEST_CASE(HitBand, "links.hit_band_is_the_same_in_every_direction", "CORE-15", test::TestFlags_None)
{
    // A link is hovered up to its thickness and 5 more from its centerline; the mouse
    // is 0.25 inside or 0.75 outside of that.
    struct Case { bool Vertical; float Thickness; float Distance; bool Hovered; };
    const Case cases[] =
    {
        { false,  1.0f,  5.75f, true }, { false,  1.0f,  6.75f, false },
        { true,   1.0f,  5.75f, true }, { true,   1.0f,  6.75f, false },
        { false, 10.0f, 14.75f, true }, { false, 10.0f, 15.75f, false },
        { true,  10.0f, 14.75f, true }, { true,  10.0f, 15.75f, false },
    };
    int wrong = 0;
    for (const auto& c : cases)
    {
        test::Storage storage;
        test::Session session(&storage);
        LinkScene scene;
        scene.Thickness = c.Thickness;
        session.DefaultContent = [&scene](int frame) { scene.Submit(frame); };
        session.Run(2);

        const auto mouse = session.Use([] { return ImFloor(ed::CanvasToScreen(ImVec2(300.0f, 300.0f))); });
        const auto at    = session.Use([&mouse] { return ed::ScreenToCanvas(mouse); });
        if (c.Vertical)
        {
            scene.Start          = ImVec2(at.x - c.Distance, at.y - 200.0f);
            scene.End            = ImVec2(at.x - c.Distance, at.y + 200.0f);
            scene.StartDirection = ImVec2(0.0f, 1.0f);
            scene.EndDirection   = ImVec2(0.0f, -1.0f);
        }
        else
        {
            scene.Start = ImVec2(at.x - 200.0f, at.y - c.Distance);
            scene.End   = ImVec2(at.x + 200.0f, at.y - c.Distance);
        }
        test::MouseMove(mouse);
        session.Run(2);

        const bool hovered = session.Use([] { return ed::GetHoveredLink() == ed::LinkId(100); });
        if (hovered != c.Hovered)
        {
            ++wrong;
            test::Check(false, "a %s link %g thick is %s at %g from it", c.Vertical ? "vertical" : "horizontal", c.Thickness, hovered ? "hovered" : "not hovered", c.Distance);
        }
    }

    test::Check(wrong == 0, "horizontal and vertical links are hovered up to their thickness and 5 from them");
}

TEST_CASE(TessellationAtZoom, "links.tessellation_follows_zoom", "CORE-15", test::TestFlags_None)
{
    // An S-shaped link in the middle of the view, 8 times larger on screen at zoom 8.
    // Without anti-aliasing a line has 4 vertices per segment at any thickness.
    const float zooms[2] = { 1.0f, 8.0f };
    int vertices[2] = {};
    for (int i = 0; i < 2; ++i)
    {
        char settings[128];
        snprintf(settings, sizeof(settings), R"({"view":{"scroll":{"x":0,"y":0},"zoom":%g}})", zooms[i]);
        test::Storage storage;
        storage.Global = settings;
        test::Session session(&storage);
        ImGui::GetStyle().AntiAliasedLines = false;
        LinkScene scene;
        session.DefaultContent = [&scene](int frame) { scene.Submit(frame); };
        session.Run(2);

        const auto center = session.Use([] { return ed::ScreenToCanvas(ed::CanvasToScreen(ImVec2(0.0f, 0.0f)) + ed::GetScreenSize() * 0.5f); });
        scene.Start = center - ImVec2(50.0f, 30.0f);
        scene.End   = center + ImVec2(50.0f, 30.0f);
        session.Run(2);
        vertices[i] = CountLinkVertices(session);
    }

    test::Check(vertices[0] > 0 && vertices[1] >= 2 * vertices[0], "the link has at least twice the vertices at zoom 8: %d at zoom 1, %d at zoom 8", vertices[0], vertices[1]);
}

TEST_CASE(CachedLinkGeometry, "links.bounds_include_curvature_and_drawn_vertices", "UIKIT-198", test::TestFlags_None)
{
    auto config = test::MakeConfig(nullptr);
    config.EnableSettingsPersistence = false;
    test::Session session(config);
    LinkScene scene;
    scene.Start = ImVec2(200, 200);
    scene.End = ImVec2(500, 200);
    scene.StartDirection = ImVec2(0, 1);
    scene.EndDirection = ImVec2(0, 1);
    scene.Thickness = 8;
    session.DefaultContent = [&](int frame) { scene.Submit(frame); };
    session.Run(3);
    const auto bounds = session.Use([] { return ed::GetLinkBounds(100); });
    test::Check(bounds.w > 250, "bounds include the curve bulging away from its horizontal endpoints: bottom %g", bounds.w);
    int vertices = 0;
    const auto geometry = test::Capture(session.DrawList);
    session.Use([&]
    {
        for (size_t i = 0; i < geometry.Positions.size(); ++i)
        {
            if (geometry.Colors[i] != c_LinkColor)
                continue;
            ++vertices;
            const auto point = ed::ScreenToCanvas(geometry.Positions[i]);
            test::Check(point.x >= bounds.x && point.x <= bounds.z && point.y >= bounds.y && point.y <= bounds.w,
                "drawn vertex (%g, %g) lies inside the cached bounds", point.x, point.y);
        }
    });
    test::Check(vertices > 0, "the geometry control observed rendered link vertices");
    session.Frame([](int) {});
    const auto omitted = session.Use([] { return ed::GetLinkBounds(100); });
    const auto unknown = session.Use([] { return ed::GetLinkBounds(999); });
    test::Check(omitted.x == 0 && omitted.y == 0 && omitted.z == 0 && omitted.w == 0,
        "a non-submitted link reports unavailable geometry");
    test::Check(unknown.x == 0 && unknown.y == 0 && unknown.z == 0 && unknown.w == 0,
        "an unknown link reports unavailable geometry");
    test::Check(bounds.z > bounds.x && bounds.w > bounds.y, "a previously returned value stays independent");
}


TEST_CASE(OutlinedLink, "links.outline_preserves_literal_color_selection_and_bounds", "", test::TestFlags_None)
{
    auto config = test::MakeConfig(nullptr);
    config.EnableSettingsPersistence = false;
    test::Session session(config);
    LinkScene scene;
    scene.Start = ImVec2(200, 200);
    scene.End = ImVec2(500, 240);
    scene.Outlined = true;
    scene.OutlineThickness = 6;
    session.DefaultContent = [&](int frame) { scene.Submit(frame); };
    session.Run(3);
    const ImU32 outline = ImColor(scene.OutlineColor);
    const auto geometry = test::Capture(session.DrawList);
    test::Check(test::FirstIndexOf(geometry, outline) >= 0, "outline vertices are emitted");
    test::Check(test::FirstIndexOf(geometry, c_LinkColor) > test::FirstIndexOf(geometry, outline),
        "literal core is painted over the outline");
    session.Use([&]
    {
        ed::SelectLink(100);
        ed::LinkId ids[4];
        test::Check(ed::GetSelectedLinks(ids, 4) == 1 && ids[0] == ed::LinkId(100),
            "outline and core share one selected identity");
        ed::GetStyle().Colors[ed::StyleColor_SelLinkBorder] = ImVec4(1, 0, 1, 1);
    });
    session.Run(2);
    const auto selected = test::Capture(session.DrawList);
    test::Check(test::FirstIndexOf(selected, IM_COL32(255, 0, 255, 255)) >= 0,
        "selection is rendered alongside the authored color and outline");
    const auto bounds = session.Use([] { return ed::GetLinkBounds(100); });
    session.Use([&]
    {
        for (size_t i = 0; i < selected.Positions.size(); ++i)
        {
            const auto color = selected.Colors[i];
            if (color != c_LinkColor && color != outline && color != IM_COL32(255, 0, 255, 255)) continue;
            const auto p = ed::ScreenToCanvas(selected.Positions[i]);
            test::Check(p.x >= bounds.x && p.x <= bounds.z && p.y >= bounds.y && p.y <= bounds.w,
                "cached bounds (%g, %g, %g, %g) contain vertex (%g, %g), color %08x",
                bounds.x, bounds.y, bounds.z, bounds.w, p.x, p.y, color);
        }
        ed::ClearSelection();
    });
    scene.Color = ImVec4(1, 0, 0, 0);
    session.Run(2);
    const auto transparent = test::Capture(session.DrawList);
    const auto automatic = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
    test::Check(test::FirstIndexOf(transparent, outline) >= 0, "a transparent core retains its outline");
    test::Check(test::FirstIndexOf(transparent, c_LinkColor) < 0 && test::FirstIndexOf(transparent, automatic) < 0,
        "explicit alpha zero never substitutes an automatic text color");
    scene.Outlined = false;
    session.Run(2);
    const auto legacy = test::Capture(session.DrawList);
    test::Check(test::FirstIndexOf(legacy, outline) < 0, "legacy submission clears the previous outline");
    test::Check(test::FirstIndexOf(legacy, automatic) >= 0, "legacy Link retains its automatic-color sentinel");
    session.Frame([&](int frame)
    {
        scene.Submit(frame);
        test::Check(!ed::LinkWithOutline(101, 11, 12, scene.Color, scene.OutlineColor, 1, -1),
            "negative outline width rejects");
        test::Check(!ed::LinkWithOutline(102, 11, 12, scene.Color, scene.OutlineColor, 1,
            std::numeric_limits<float>::infinity()), "infinite outline width rejects");
        test::Check(!ed::LinkWithOutline(103, 11, 999, scene.Color, scene.OutlineColor),
            "missing endpoint rejects");
    });
}
