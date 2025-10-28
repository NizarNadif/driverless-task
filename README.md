# Car Simulation in Unreal Engine 5.6

![Demo](task-demo.gif)


## Setup
In order to manage the project with ease, even if the official libraries indicate to ignore them, it's important to manage the code warnings presented by default. The underlying problem with these warnings is that they clog the error list, because they're also notified as errors. To hide them, a suppression must be done at engine level, by creating a ```Directory.Build.props``` file in ```EpicGames/UE_<EngineVersion>/Engine/Source/Programs```, where the ```EpicGames``` directory is the root directory of the Unreal Engine installation. . The content of the file must be as follows:
```xml
<Project>
	<PropertyGroup>
		<!-- Suppress NuGet and SDK warnings -->
		<NoWarn>$(NoWarn);NU1701;NU1504;NETSDK1179</NoWarn>

		<!-- Downgrade specific warnings from errors to messages -->
		<WarningsNotAsErrors>$(WarningsNotAsErrors);NU1605</WarningsNotAsErrors>

		<!-- Don't treat warnings as errors for auto-generated projects -->
		<TreatWarningsAsErrors>false</TreatWarningsAsErrors>
	</PropertyGroup>
</Project>
```

Moreover, it's suggested to install the 8th version of .NET SDK, in order to avoid compatibility issues with the Unreal Engine build system. If done, the ```global.json``` file in the root directory of the project will ensure that the correct SDK version is used.	

### Running the Project
The project could be run from the Unreal Engine Editor, by opening the ```DigitalTwin.uproject``` file in the root directory of the project. Once opened, it's necessary to build the project, by either building the solution in Visual Studio or by clicking the ```Build``` button in the Unreal Engine Editor. Once built, it's possible to run the project by clicking the ```Play``` button in the Unreal Engine.

An alternative to the moving player's POV is a vehicle, that can be set up in the GameMode Override, under the World Settings, to  ```BP_VehicleAdvGameMode```. If the gamemode is changed to the vehicle one, the player start must be moved to the start of the interested track.

## Implementation
The projects implements a simple Digital Twin application, where a track is visualized in a 3D environment, and a car moves along it.
Moreover, the track is randomly populated with obstacles, cones as default, and its perimeter is bounded by walls. So, the car tries to follow the track while avoiding these obstacles.

## Track
The tracks are defined using splines. The mesh used is provided by Unreal Engine, and modified to include walls on its sides.

When the application gets executed, the tracks are randomly populated with obstacles, as said earlier.
The spawner is a simple C++ actor that need some parameters, among which there's the track itself.

## Car
The car model is imported from the Unreal Engine vehicle templates, and modified to fit the use cases. In detail, the car has been modified to remove the simple collision logic, and the manual command inputs. Moreover, it's possible to follow the AV on the first track by changing the world settings, by setting the GameMode Override to the custom GameMode provided in the project.

## Movement Logic
The movement logic is implemented in C++, as a plug-and-play module to be attached to any vehicle model. In particular, the movement logic uses both the spline of the track and a simple ray-tracing logic to ensure that the car stays on the track and avoids obstacles. The ray-tracing logic is based on three probes casted in front of the vehicle: one in the center and two on the sides. If an obstacle is detected by any of these rays, the car adjusts its steering to avoid it. The parameters of the movement logic are easily tweakable from the editor.

### Forward movement
As said earlier, the car moves forward along the spline of the track. The forward movement is implemented by calculating the direction of the spline at the car's current position, and considering the avoidance logic.

### Stuck State
If the car gets stuck, for example against a wall, an unstuck procedure is triggered. The vehicle reverses for a short distance, the it realigns for a bit and the _"regular"_ logic is resumed.

The logic is really primitive and can be improved in many ways, but it works for the purpose of this task. Many times it may happen that the car goes over an obstacle, because the velocity isn't as adaptive as it should. This can be improved by implementing a more complex velocity control logic. Moreover, hard turns are not well handled because of the simplicity of the velocity system, so the vehicle hits the walls and gets stuck more often than it should, as can be seen in the *second track* of the demo.

## Debug / Telemetry
For each vehicle, a simple debug system is implemented. To be more specific, some generic telemetry data are printed on the screen, whilst the vehicle's target is visualized in the 3D environment using debug spheres. The vehicle's actually followed path is also visualized using debug lines.

## Future Works
As said earlier, the movement logic can be improved in many ways, with more complex algorithms for both path following and obstacle avoidance. Moreover, the perception system could also be improved, passing from the actual ray-tracing logic to a LiDar system, in order to obtain point-cloud data. On the LiDar manner, there are some implementations online, the most notable are:
1. [LiDar Toolkit](https://dl.acm.org/doi/pdf/10.1145/3708035.3736025): this paper indicates an implementation of a plugin that could be used in Unreal Engine to simulate the sensor following a real LiDar behavior. Unfortunately, it was not possible to integrate it in the project due to time constraints, in particular because of the need to request access to the plugin itself.
2. [MetaLiDar](https://github.com/metabotics-ai/MetaLidar): this is an open-source LiDar simulator for Unreal Engine, that can be used to simulate a LiDar sensor. It has not been integrated becauase the setup used in this project is Windows-based, whilst the plugin works only on Linux systems.
