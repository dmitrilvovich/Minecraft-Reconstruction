**Minecraft Reconstruction (mcr)** is a personal research project I'm working on to reconstruct Minecraft structures from one or more 2D images and eventually export the result as an actual 3D Minecraft build/schematic.

Right now, I'm mostly figuring out how I want to approach the problem before I start building everything out. I'm currently looking into things like discrete inverse rendering, constraint-based reconstruction, using multiple images to score different possible blocks/states, front-to-back visibility through the Minecraft grid, ray-based constraints, and recovering the camera position/orientation and Minecraft lattice from the images. I'm also trying to figure out how much of the reconstruction can be solved directly before I need to start doing more expensive search over ambiguous block shapes/states.

The project will be written primarily in C++.
