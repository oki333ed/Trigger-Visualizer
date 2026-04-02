# v2.2.1
## Visual Refresh Patch
- Redrew **shader trigger sprites**
- New **logo**
- Updated **mod description**

# v2.2.0
## Dynamic Texture Expansion & Improvements
- Added **new dynamic textures for Area triggers**
- Added **4 new trigger textures**
- Updated **10 existing trigger textures**
- Small **settings adjustments**
- Updated **mod description**
- Fixed various **bugs and issues**

# v2.1.0
## Dynamic Texture & Settings Update
- Added **two new dynamic textures**
  - Pulse trigger - displays its color
  - Color trigger - displays its color
- Added **sprites for old Color triggers**
- Updated **Event trigger sprites**
- Small **settings adjustments**
- Fixed a **crash on Android** that occurred when enabling Free Mode on portals
- Fixed an **iOS startup issue** where the game would not launch with the mod enabled
- Various minor fixes and tweaks

# v2.0.1
## Mini patch
- Added one dynamic texture (stop trigger)
- Small changes in the code

# v2.0.0
## Massive Visual & Performance Update
- Reworked **131 trigger textures** to match original trigger resolutions  
  (near 1:1 accuracy with vanilla sizing)
- Enhanced **21 trigger textures** with improved visuals and alignment
- Added **7 dynamic textures**
  - Move trigger
  - Rotate trigger
  - Count trigger
  - Pickup trigger
  - Spawn trigger
  - Collision trigger
  - Gravity trigger
- **~5x performance improvement** from a partial rewrite and smarter update logic
- Large optimization pass
  - Reduced overhead
  - Fewer refresh cycles
  - Smoother editor performance
- Dynamic texture toggle moved to settings  
  (editor button removed)
- Dynamic item settings merged into a unified system
- Dynamic textures now update automatically on placement, copy, and setting changes
- Updated Geode to the latest version
- Added support for game version **2.2081**

# v1.6.1
## Micro patch
- FPS message now appears **only once**

# v1.6.0
## Dynamic Performance & Texture System Update
- Added **dynamic texture for StartPos** with ~2300 unique variations depending on settings and context
- Added **dynamic textures for Camera triggers**, reacting to their parameters
- Updated **collision block textures** with clearer visuals and better editor readability
- **Major code optimization**
  - Reduced editor draw calls
  - Optimized dynamic texture evaluation logic
  - Lower memory usage for texture caching
- General code cleanup and **refactoring** for better maintainability and stability
- Added **FPS monitoring system**
  - Detects low FPS situations
  - Displays a warning suggesting disabling certain dynamic textures

# v1.5.0
## Visual Refresh Update
- Added **5 completely new trigger sprites**
- Updated and replaced **25 old trigger sprites**
- Improved overall visual consistency between trigger categories
- Minor tweaks to sprite alignment and scaling in the editor

# v1.4.2
## Editor Interaction Fixes & Tweaks
- Added **dynamic texture for Item Edit triggers**
- Added **two new settings** for Item Edit textures
- Updated some trigger textures
- Dynamic button now disappears during Playtest

# v1.4.0
## Dynamic Texture Expansion
- Added new dynamic textures
  - UI trigger
  - Item comp trigger
- Added new settings to configure dynamic textures
  - Enable Item comp dynamic texture
  - Enable UI dynamic texture
- Added a button in the editor to **toggle dynamic textures**

# v1.3.3
## Smart Event Composition
- Added a new feature: **automatic texture combining** for Event triggers with multiple actions
- Added a new setting to enable or configure this behavior

# v1.3.2
## Visual Settings Overhaul
- Added **Visual settings** section

# v1.3.1
## Trigger Sprite Settings
- Added new trigger textures
  - Song
  - Song Edit
  - Shake
- Added new settings
  - Reset StartPos trigger texture
  - Reset Shake trigger texture
  - Reset Area Stop trigger texture

# v1.30
## Dynamic Behavior Update
- Added **dynamic texture updates** for SFX and Event triggers
  - SFX textures now change depending on volume
  - Event textures change based on their settings
  - Settings allow disabling dynamic updates
  - Added **volume threshold setting** for SFX triggers
- Added several new textures for triggers that previously had no custom visuals

# v1.20
## Camera & Visual Additions
- Added new **Camera trigger texture**
- Added several new trigger textures
- Added new settings

# v1.10
## Trigger Visualizer
- Added new trigger textures
  - Gameplay triggers
  - Area triggers
  - Logical triggers
- Added mod settings
  - Toggle texture replacement per trigger category
- Fixed shader trigger icon offset in the editor
- Improved overall editor visual clarity

# v1.0.1
## Platform Compatibility Patch
- **Cross-platform release**

# v1.0.0
## Shader Visualizer
- Initial release
