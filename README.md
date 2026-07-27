# OPEN MCDU

An open-source A320 MCDU (Multipurpose Control and Display Unit) simulator. Built in C++20, uses SFML 3 for rendering. The MCDU follows the ARINC 14x24 character grid standard used in real Airbus aircraft.

## Features

- ARINC-compliant 14x24 character LCD grid with per-cell color and font size
- Real bezel button layout with pixel-accurate hit detection (measured from reference image)
- LSK/RSK side keys, function keys, action keys, and numeric keypad
- Scratchpad input with CLR logic (clear char, CLR mode, message display)
- Nav database parser for X-Plane .dat format (fixes, VORs, NDBs, airports)
- FROM/TO validation against real nav database
- CRZ FL/TEMP auto-format (FL300, ISA temperature interpolation)
- F-PLN page with circular scrolling and EDIT mode (deep-copy, ERASE/INSERT, YELLOW coloring)
- No LSK slot arrays or maps -- pages just answer "what field is at this button?" through a virtual method
- Rendering backend can be swapped (only MCDU::render() touches SFML)
- All pages and FMGC logic are rendering-agnostic

### How buttons work

Press a bezel button. MCDU routes it to PageStateMachine. PageStateMachine asks the current page: what field is at (side, index)? The page returns a ClickHandler with a data pointer and an onClick callback. PageStateMachine dispatches scratchpad content to the callback.

No slot arrays. No per-frame zeroing. Pages override a single virtual method:

```cpp
const ClickHandler* getClickHandler(int side, int lskIdx) override;
```

Side is 0 for left (LSK1-6) or 1 for right (RSK1-6). lskIdx is 0-5. Return nullptr for nothing there.

### The 24x14 grid render system

The MCDU LCD is a 14-row by 24-column character grid (ARINC 739 standard). Each cell has:

- A Unicode codepoint
- A color (GREEN, AMBER, WHITE, CYAN, YELLOW, BLUE, DIM)
- A font size (22pt for data, 14pt for labels)

Pages write to a ScreenBuffer. MCDU::render() walks the buffer and draws each cell using SFML. The bezel image is drawn underneath as a texture.

### Display colors

| Color  | Use                                    |
|--------|----------------------------------------|
| GREEN  | Data, placeholder dashes               |
| AMBER  | Warnings, empty fields, edit controls  |
| WHITE  | Labels, normal text                    |
| CYAN   | Filled data fields                     |
| YELLOW | EDIT mode (temp flight plan)           |
| DIM    | Separators                             |

### How the grid maps to buttons

Each LSK/RSK button sits next to a data row on the screen. The mapping is:

- LSK1 / RSK1 -> row 2 (data)
- LSK2 / RSK2 -> row 4
- LSK3 / RSK3 -> row 6
- LSK4 / RSK4 -> row 8
- LSK5 / RSK5 -> row 10
- LSK6 / RSK6 -> row 12

Rows between data rows (3,5,7,9,11) carry labels and small text. Row 0 is the title row. Row 13 is the scratchpad line.

## Flight plan data model

The flight plan is a doubly-linked list of FlightWaypoint nodes. Each node has prev/next pointers for O(1) insert and remove. Marker nodes handle DISCONTINUITY and END OF F-PLN display.

EDIT mode works in three steps:
1. beginEdit() -- deep-copies the active plan
2. User edits the copy (insert waypoints, remove discontinuity)
3. commitEdit() -- swaps edit into active, or cancelEdit() -- discards edit

The F-PLN page uses this for waypoint insertion. When you add a waypoint at the discontinuity, the page enters edit mode, rendering switches to yellow, and LSK6/RSK6 show <-ERASE and INSERT*.

Scrolling is circular. The viewport always shows 5 items. With fewer than 5 items, they shift through the 5 viewport slots. With 5 or more, the window moves through the plan.

## Nav database

The NavDatabase class loads waypoints from X-Plane 12 format files and airports from OurAirports CSV. Loading is thread-safe (mutex-protected merge into main map).

| File             | Contents                       |
|------------------|--------------------------------|
| earth_fix.dat    | Waypoints (lat, lon, ID)       |
| earth_nav.dat    | VORs and NDBs                  |
| airports.csv     | ICAO airport codes             |

These files are not in the repo. Copy them from your X-Plane 12 installation (Resources/default data/) or download airports.csv from OurAirports.

## Making a new page

1. Create a header in src/Pages/
2. Extend the Page class (defined in MCDU/Field.h)
3. Implement buildScreen() to render your layout
4. Implement getClickHandler() to bind buttons

```cpp
class MyPage : public Page {
public:
    MyPage(FMSDataStore& store) : m_store(store) {
        m_myFieldH = {&m_store.someField, nullptr, storeString, &m_store.someField};
    }

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        if (side == 0 && lskIdx == 0) return &m_myFieldH;
        return nullptr;
    }

    void buildScreen(ScreenBuffer& buf) override {
        buf.setString(0, 0, "MY PAGE", CellColor::WHITE);
        FieldRenderer::render(buf, Field::BOX, 2, 0, 10, 0,
                              m_store.someField, CellColor::CYAN);
    }

private:
    FMSDataStore& m_store;
    ClickHandler m_myFieldH;
};
```

Then register in MCDU constructor:
```cpp
m_stateMachine.registerPage("MY", std::make_unique<MyPage>(fmgc.dataStore()));
```

The ClickHandler struct has four fields:

- dataPtr: pointer to a string the handler reads/writes
- slashTarget: pointer to a left/right pair (for fields like FROM/TO)
- onClick: callback function (validates, stores, returns error message)
- clickCtx: context pointer passed to the callback
- isDirectAction: if true, the handler fires without consuming scratchpad

Common handlers are provided: storeString (writes scratchpad to a string), storeSlash (splits on /), storeSlashValidated (with nav database lookup), storeCrzFlTemp (formats FL + temperature).

## Building

Requirements:
- CMake 3.20+
- C++20 compiler
- SFML 3 (install via vcpkg)

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -G "Visual Studio 17 2022" -T host=x64 -A x64
cmake --build build --config Debug
```

The CMakeLists.txt uses GLOB_RECURSE so new files in src/ are picked up automatically. SFML DLLs are copied to the output directory on Windows.

## Asset credits

- MCDU bezel image: original artwork
- B612 font: PolarSys / Airbus, SIL Open Font License. Modified to add U+25AF box marker character.
- Nav database files: X-Plane 12 (Laminar Research). Not included. Respect their license terms.
- Airports CSV: OurAirports by David Megginson, public domain.

## License

GNU General Public License v2. See LICENSE.
