# Digital Twin Recruitement Task for EagleTRT

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