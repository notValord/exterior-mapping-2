# Exterior Mapping 2

This project is a continuation of the work by Boris Burkalo:  
https://github.com/boisbb/ExteriorMapping

Exterior Mapping is an image-based rendering algorithm inspired by light fields. It reconstructs novel views from a set of reference images, using depth buffers and camera intrinsics. The core idea is to reuse data from pre-rendered views to synthesize new perspectives of a scene with photorealistic detail, while avoiding the high computational cost of traditional rendering.

A key advantage of this approach is that the rendering cost depends primarily on image resolution rather than scene complexity (geometry or lighting). This makes it particularly suitable for large-scale scenes with high geometric detail, where image-based techniques can outperform methods like ray tracing. A typical use case is an “inside-out” view of expansive environments, which is where the name *Exterior Mapping* comes from.

Although this project builds on previous work, the implementation was written from scratch (the original code was only used as a reference for certain algorithmic details). As this is also my first project using the Vulkan API, the initial project structure was based on the Vulkan tutorial (https://vulkan-tutorial.com/), and has since been extended independently.

## Project structure

- `/debug` – Stores novel view snapshots and offline image captures  
- `/headers` – Header files  
- `/include` – External libraries and dependencies  
- `/resources` – Models, textures, and other assets  
- `/setups` – Configuration files for testing
- `/shaders` – Shader programs  
- `/src` – Source code

## Dependencies and Requirements

This project is written in **C++17** and uses **Vulkan 1.1** with subgroup operations in shaders.

It was developed and tested on Linux using an **NVIDIA GeForce GTX 1050**.  
The compute shaders assume a **subgroup (warp) size of 32**, and are currently tailored for NVIDIA GPUs. Other hardware (e.g. AMD) is **not supported** without modification.

### Build Requirements
- A C++17-compatible compiler (e.g. GCC, Clang, MSVC)
- CMake (recommended for building the project)
- Vulkan SDK (including headers, loader, and validation layers)

### Runtime Requirements
- A GPU with Vulkan 1.1 support
- Vulkan-compatible drivers

### External Libraries
External libraries are included in the `/include` directory:

- **ImGui** – User interface  
- **Vulkan Memory Allocator (VMA)** – GPU memory management  
- **tinyobjloader** – Wavefront OBJ model loading  
- **tinyexr** – EXR image support  
- **stb_image** – Image loading  
- **nlohmann/json** – JSON serialization  

## Build and Run

From the project root directory:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

If the build is successful, run the application with:

```bash
./ExteriorMapping
```

## GPU Requirements and Selection

At startup, the program prints diagnostic information, including the selected GPU device.

The application requires a GPU that supports:
- The `VK_KHR_swapchain` extension  
- A valid swapchain (supported surface format and present mode)  
- Sampler anisotropy  
- Queue families supporting **graphics**, **compute**, and **presentation**

During device selection, preference is given to **dedicated GPUs**, particularly those that provide a separate **transfer queue family**. However, a dedicated transfer queue is not used at the moment to minimize overhead and potential issues, given the uncertain performance benefit.

## Controls and Interface

### Camera Controls
- Move camera: **WASD** or **Arrow keys**
- Move up: **Space**
- Move down: **Left Shift**
- Rotate camera: **Right mouse click + drag**
- Adjust movement speed:
  - **Q** – increase speed
  - **E** – decrease speed

### Keyboard Shortcuts
- **M** – Toggle novel camera / reference cameras 
- **B** – Toggle observer / reference cameras
- **N** – Switch to next reference camera (when enabled)
- **U** – Toggle ImGui UI overlay
- **P** – Capture a snapshot of the current view

Note: Keyboard controls are disabled while the ImGui UI is focused to avoid conflicts with text input.

## Interface

The interface is divided into several headers:

### General
Contains general scene setup controls, including scene selection and scaling, light position, resolution settings, snapshot capture, and setup controls.

### Cameras
Visualizes camera controls, including the currently active camera, its position, orientation, and movement speed.

For reference cameras, it allows:
- Adding a new reference camera at position (0, 0, 0)
- Deleting the last reference camera

### Offline Render

Contains options to render images and depth buffers from reference cameras, which are required for the novel rendering pipeline. Results can be saved to files or visualized on screen for debugging purposes.

In novel and observer views, all reference views are displayed in a grid. When viewing from a reference camera point of view, only the image for the selected view is shown.

### Novel Render
Contains controls for the two novel view rendering algorithms.

- **Render Novel View**: Implements the same algorithm as in *Exterior Mapping 1*, using three heuristics and an additional pixel selection in a cone.
- **Novel Analytic Synthesis**: An analytical approach more similar to point cloud-based methods.
- **Depth Settings**: Settings related to the cone-marching-like algorithm, designed to mitigate ghosting artifacts.

More details are available in the *Novel View Rendering* section.

### Debug
Displays FPS and additional debug visualization options, including:
- Depth view
- Reference cameras placeholders
- Reference view frustums
- Novel ray intersections used in rendering
- Point cloud visualization

## Scenes

The application uses `.obj` models with `.mtl` material files. Materials can define either a solid color or a texture. The shading uses a simple lighting model and does not include bump maps or other advanced material maps.

The `/resources/models` directory currently contains three test scenes:
- **city** – created in Blender
- **porsche** – the same model used in *Exterior Mapping 1* for visual comparison  
- **viking_room** – taken from the Vulkan tutorial  

### Adding New Models

New models can be added by placing their `.obj` and `.mtl` files into the `/resources/models` directory and restarting the application. The models will be automatically loaded and listed in the scene selection combo box.

Texture files should be placed in `/resources/textures`. The `.mtl` files must reference textures using paths relative to this directory.

## Setups

When the application starts, the initial configuration is always the same: the default scene is loaded and all cameras are positioned at (0, 0, 0).

To simplify scene configuration, setups allow the user to save the current state—including the scene, lighting, resolution, and camera configuration—under a unique name, and load it later.

Setups are stored as JSON files in the `/setups` directory and can also be used for testing purposes.

## Implemented algorithms

### Novel View Rendering (Exterior Mapping 1)

This method corresponds to the algorithm implemented in *Exterior Mapping 1*.

The novel view rendering algorithm reconstructs an unknown view from a set of reference images, based on the relative positions of the cameras and their projection matrices. It is inspired by light field techniques, with additional utilization of depth information.

For each pixel, a viewing ray is generated and sampled uniformly based on a predefined sample count. The sampling range is defined by the region of greatest overlap between the viewing frustums of the reference cameras for the given ray.

Each sample along the ray is evaluated using a selected heuristic. The best sample is selected, and the final pixel color is computed as the average of the corresponding pixels from all reference cameras.

Once all pixels are processed, the resulting novel image is presented on the screen.

### Heuristics

The algorithm evaluates ray samples using three heuristics:

#### Color Heuristic
This heuristic is based on the assumption that the correct sample should produce similar colors across all reference images.

In other words, in the ideal scenario different colors typically correspond to different objects. For the correct sample, rays from all reference cameras intersect the same point in space and therefore yield similar color values.

The heuristic computes the color consistency by measuring the difference between the maximum and minimum values for each color channel across all reference images, and summing these differences.

This heuristic does **not** utilize depth information.

**Possible issues:**
- Objects with similar colors may lead to incorrect matches  
- Highly detailed or noisy textures can reduce reliability  
- Large uniformly colored surfaces can produce ambiguous results 
- Background elements (e.g. skyboxes) can dominate and produce incorrect samples  

---

#### Depth Heuristic

Since color is not always a reliable metric, the depth heuristic operates directly on geometric consistency using depth information from the reference images.

For a correct sample, projections from all reference cameras should correspond to the same point in 3D space. Therefore, the distance between reconstructed points should be close to zero.

Each sample along the ray is projected into each reference view and the corresponding pixel is selected and back-projected into world space using the depth buffer.

Two distance metrics can be used:
- **Point-to-point distance** – distance between the sampled point and the reconstructed point  
- **Point-to-ray distance** – distance between the reconstructed point and the viewing ray  

The distances from all reference cameras are accumulated, and the sample with the lowest total distance is selected. The final pixel color is then computed as the average of the corresponding pixels from all reference images.

**Possible issues:**
- The sampling count must be high enough to ensure that at least one sample is close to the true intersection point in the scene.
- Depth discontinuities (e.g. object edges) and occlusions may produce artifacts, as not all cameras observe the same point, which can lead to ghosting.
- For good results, most cameras must observe the same scene point for the heuristic to remain valid, which requires redundancy in the input data.

---

#### Angle Heuristic

This heuristic is similar in concept to the depth heuristic, but instead evaluates the **angular distance** between the reference point and the viewing ray. This causes the metric to be relative to the distance from the camera, effectively forming a cone-like region around the ray. Samples that lie closer to the ray direction yield lower error values.

However, this approach can struggle with points that are close to the ray direction but located far from the current sample along the ray.

**Possible issues:**
- Ambiguity for distant points aligned with the ray direction  
- Reduced precision compared to depth-based evaluation  
- Similar limitations as the depth heuristic  

---

### Algorithm Challenges

The issues with the depth heuristic are difficult to resolve due to the inherent nature of the algorithm. When the final color of a pixel is averaged across all reference cameras, depth discontinuities are often blurred, leading to ghosting artifacts. This happens because the true intersection in the novel view may be occluded in the reference views, causing inconsistencies when the depth is averaged across multiple cameras. To mitigate this issue, we have the posibilities:
  
- Use only the *n* best reference cameras for color averaging, excluding outliers.
- Use only the reprojected pixels that fall within the pixel frustum.

### Solution 1: Best `n` Reference Cameras

One solution to this problem is to select only the **best `n` reference cameras** for each sample. By focusing on the most relevant cameras, we can minimize the fading effect caused by irrelevant or occluded reference views, as they wouldn't be included if better approximations exist.

### Solution 2: Reprojection into a Frustum or Cone

An alternative approach involves working only with **reprojected pixels** from the reference cameras that fall into a frustum projected by the novel view's pixel. The pixels that reproject outside of the frustum area are ignored and won't affect the result value, mitigating the ghosting artifacts. For easier implementation, we consider a **cone shape** instead of a frustum, with the ray being the center axis.

For this method the depth distance between the reference camera and the novel view pixel is computed as a **point-to-point distance** specifically.

### High Sampling and Neighborhood Approach

However without the sample count not being large enough, the likelihood of a reprojected sample falling directly into the pixel cone is low. To address this we expand the sampling area to include a **larger neighborhood** around the novel pixel. This approach is similar to **cone marching**, where we sample a set of reference pixels around the target.

### False Positives

While this removes misleading samples, it introduces the risk of **false positives**. A false positive occurs when a pixel from a single reference camera falls into the pixel cone, and its color is erroneously set as the final value, even though the majority of the cameras (including the novel view and most reference cameras) do not see the object anywhere near that location.

To mitigate false positives, we introduce a **threshold percentage** of cameras that must agree on the sample. Specifically, a minimum percentage of cameras must reproject a pixel into the cone for the sample to be considered valid. This helps ensure that the selected color comes from a more reliable set of reference cameras.

### Fallback to Original Depth Heuristic

For pixels that do not have enough precise samples from the neighborhood cone, the algorithm **falls back to the original depth heuristic** to compute the color. This ensures that color is still computed, even if a precise match is not found within the cone-based sampling region.


**In the end, this setup isn't reliable enough to provide consistently good visual results**. It requires finding the perfect balance between sampling precision, cone neighborhood size, and camera percentage threshold. Given these challenges, the approach remains difficult to optimize and inconsistent for high-quality outcomes.

---

### Novel Analytical Synthesis

This algorithm builds on the concept of novel view rendering, but rather than sampling rays in space—which can struggle with precision—it works analytically with the ray.

#### **Ray Projection and Pixel Candidate Selection**

Each ray, per pixel, is projected as a whole into the reference camera views. After this projection, the algorithm checks for **line-pixel intersections** between the ray and the camera views.

- **Intersection Check**: If a pixel is intersected, and the depth of the pixel is sufficiently close to the depth of the ray at that point, the pixel is considered a potential candidate.
- **Candidate Selection**: Among all potential candidates, the algorithm searches for the one where the ray intersects first across all cameras.

This ensures that the selected pixel is the one that the ray encounters earliest in the 3D space, providing a more accurate representation of the novel view.


The pixel grid between different camera views does not align, making it difficult to directly map corresponding pixels across reference images. Therefore, the algorithm avoids cross-view pixel mapping by **selecting the final pixel from only one reference camera**—the one that sees the pixel value closest to the ray’s origin.

This simplifies the process and ensures that the final color is consistently derived from a single reference camera.

#### **Depth Pyramid Optimization**

Brute-force checking of every pixel for intersections would severely impact performance. To optimize this, the algorithm uses a **depth pyramid** structure created from the depth buffer.

- **Pyramid Structure**: The depth pyramid consists of **4 levels**, where the highest level is constrained to a **1024x1024 resolution** (the original depth buffer resized).
- **Lower Levels**: The lower levels store the **minimum and maximum depth values** for each tile, with the tile size reduction alternating between 8x4 and 4x8 to match the 32 warp size used by NVIDIA GPUs.
- **Efficiency**: The lowest level contains 32 tiles, meaning a single warp can process the entire pyramid for each ray and camera view.

#### **Intersection Condition and Final Pixel Selection**

The algorithm checks for intersections between the ray and the tiles at each level of the pyramid:

1. **2D Line Intersection**: The 2D line representing the ray is checked for intersection with a tile.
2. **Depth Check**: If the 2D intersection is successful, the ray's depth segment is compared against the depth range stored in the depth pyramid.
3. **Final Pixel**: After processing the pyramid structure, the algorithm determines the first pixel that the ray intersects in both **2D space** and **depth space**.

The novel image then contains the closest value from all reference cameras for the given ray, ensuring a more precise representation of a real point of intersection with the scene.

#### **Visual Characteristics and Comparison**

The resulting visual is more similar to a point cloud than a light-field rendering. Since the result value is only taken from one reference view, it avoids ghosting artifacts and doesn't require the data duplication necessary for original novel view rendering. The algorithm does have issues with aliasing, due to both depth resolution and the resolution of the novel view. Compared to point clouds, it doesn’t suffer from flickering or z-fighting per sample, nor does it exhibit issues with point size (i.e., seeing gaps between points when viewed from close up). However, the image will still appear pixelated.

For parts of the scene that aren't visible from any reference camera, the algorithm behaves like a point cloud where no data is available. More views would need to be added, or a specialized algorithm would have to be used to fill in these regions. Currently, these areas are filled with a constant color.

The resulting visual is more similar to a **point cloud** than to traditional light-field rendering. Since the final value is taken from a single reference view, it avoids ghosting artifacts and does not require the data duplication needed in original novel view rendering methods.

- **Comparison to Point Clouds**:
  - It doesn’t suffer from flickering or z-fighting per sample.
  - It doesn’t have issues with point size (i.e., gaps between points when viewed from close up).
  - However, the image still appears **pixelated**, as it cannot represent smooth surfaces in high detail.


One visual deficiency is that the algorithm struggles with **aliasing** due to the interference between the depth map resolution and the novel view resolution.


#### **Handling Unseen Regions**

Regions of the scene that are not visible from any reference camera are currently filled with a constant color. The algorithm lacks the necessary data to estimate the resulting intersection for these areas. 

To address this, a possible solution would be similar to a **point cloud** approach, where:
- **More reference views** could be added to cover previously unseen areas.
- Alternatively, a **specialized algorithm** could be developed to fill in these regions with more realistic data, potentially using techniques like inpainting or interpolation from neighboring views.


## General Workflow

1. **Setup**: 
   Set up the scene, camera, and resolution.

2. **Prepare Reference Images**: 
   Take snapshots of the offline rendered images to prepare for novel rendering.

3. **Novel Rendering**:
   Perform the novel rendering, either using the original algorithm or the analytical solution.

### Novel Rendering Options
The novel view rendering includes additional options to mitigate ghosting artifacts using the cone-line marching algorithm.

- **Sample Count**: Defines the number of samples for rendering.
- **Current Sample**: A debug tool to view the result from one exact sample. If set to the total number of samples, the algorithm performs standard novel rendering, selecting the best sample per pixel.

#### Depth Settings
Only applicable when the **depth heuristic** is selected and should use point-to-point distance metric.

- **Cone Option**: Does not attempt to mitigate ghosting artifacts.
- **Only in Cone**: Displays only the samples for which a result was found within the cone. The remaining samples are marked with a bright pink background.
- **Conditional in Cone**: Fills the pink areas (where no result was found) using the classic depth heuristic approach, averaging depth across all cameras.

- **Neighborhood Size**: Specifies the number of surrounding pixels considered for the cone’s divergence.

- **Cone Percentage**: Sets the threshold of reference views required to find a valid sample within the cone for it to be considered valid.

> **Note**: To change the reference camera views (number, position, etc.), stop the novel view rendering and re-render the offline images with the updated camera configuration.

> **Note**: Changing the reference camera position, adding or deleting cameras, or resizing the window during novel rendering (or without re-rendering the offline images) can lead to **errors** in uncovered scenarios, as resources need to be reallocated. If this happens, you can try to report it as an issue.


## Known Issues

- Transparent objects are not rendered correctly, requires separate draw cycle.
- The image is invalid after point cloud rendering, resize -> new synthesis.
- Offline images are not correctly presented after point cloud rendering:
  - A command buffer expects an image in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, but it is still in `VK_IMAGE_LAYOUT_GENERAL`.

### TODO

- Automatically load scene files from the directory.
- sort and take best n cameras to get the colro fro normal nvoel render

### Limitations

- All operations (graphics, compute, and transfer) are currently executed on the **graphics queue**:
  - This reduces potential parallelism.
  - It assumes that the graphics queue also supports compute operations. (ISSUE)
  - While this works on the tested hardware, it may not be portable across all devices.

## License

This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.

## Notes

- The minimum number of reference cameras is set to **2**, as fewer views are not sufficient for the application.
- The maximum number of reference cameras is limited to **32** due to data transfer constraints when passing camera data to the compute shaders.
- The maximum number of materials per scene is limited to **60**, due to shader or buffer size constraints.