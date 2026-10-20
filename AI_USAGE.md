# AI USAGE
In order to stay true of what has been done, here is what AI has been used to do in this code. Are mentioned here the actions that were done by an AI, not when it was used as a <a href="https://en.wikipedia.org/wiki/Rubber_duck_debugging">duck</a>.

## Dynamic Weapons
* No Usage
## Vulkan SP Renderer
AI has been used in the process of converting the Vulkan MP Renderer from <a href="https://github.com/JKSunny/EternalJK">EternalJK</a>. Here is what it was told to do :
### Small fixes and improvements
* Checking the integration of the MP Renderer done by Arbiter.
* Copying the MP Renderer to SP and updating the build pipelines (Makefiles) 
* Copying the Missing methods using Vanilla renderer as a base
* With developper input, Fixing thing that were not thought of when the new Renderer was generated. 
* Adjusting the dynamicglow that broke when Vulkand was started first
### Stencil Shadows
* Adding/Restoring stencil shadows (cg_shadows 2) in the Vulkan SP Renderer.
* Adding/Restoring Volumetric shadows (cg_shadows 3) in the Vulkan SP Renderer.
* With developper input, adding a GPU variant (cg_shadows 4) that moves the silhouette work to a geometry shader.
