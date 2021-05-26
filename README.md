# alekhana

alekhana (ālēkhana in ISO‐15919) is a typeface rendering tool, used for testing. It supports TrueType and OpenType font files and Unicode text through multiple renderering engines.

The rendered bitmaps can then be compared and diff'ed against master images through scripts provided.

Supported platforms: MacOS (Qt6 and CoreText engines), Windows (DirectWrite engine).

## History

I implemented this originally to help test [Sampradaya](https://github.com/deepestblue/Sampradaya), but split it out into its own project when I figured it may be independently useful.

## Licensing

On Windows, this tool depends on and bundles Microsoft's [TestApi library](https://github.com/microsoft/TestApi), released under the [Microsoft Public License](https://github.com/microsoft/TestApi/blob/master/LICENSE). The rest of the software is under the [AGPL‐3.0](https://github.com/deepestblue/alekhana/blob/main/LICENCE).

## Usage

### On MacOS

1. If you prefer to grab pre‐built binaries, grab the [latest binaries](https://github.com/deepestblue/alekhana/releases/download/latest/alekhana_dist.zip), unzip them and change into the unzipped directory. Skip to Step 7.
1. Grab the [latest tar.gz sources](https://github.com/deepestblue/alekhana/archive/refs/tags/latest.tar.gz).
1. `tar xzvf alekhana-latest.tar.gz`
1. `cd alekhana-latest/macos`
1. `make dist`
1. `cd ../dist`
1. The current directory contains the `alekhana` runtime artifacts.
1. The intended workflow is to use the Bash script `generate_images` to generate a set of master images, and then to use `run_tests` to compare the rendering using the current typeface file against the masters.
1. The test cases need to be organised as text files, all in a flat directory. Each line in each text file is a test string.
1. To generate masters, use `generate_images` as below:

    ```./generate_images -d <OUTPUT_DIRECTORY_FOR_MASTER_IMAGES> -c <DIRECTORY_OF_TEST_CASES> -t <PATH_TO_FONT_FILE> -r <PATH_TO_RASTERISER_BIN>```
As an example,

    ```./generate_images -d ../masters/coretext -c ../cases -t ../src/acme.ttf -r ./rasterise_text_coretext```
1. To run tests, use `run_tests` as below:

    ```./run_tests -e <EXPECTED_MASTER_IMAGES> -c <DIRECTORY_OF_TEST_CASES> -t <PATH_TO_FONT_FILE> -r <PATH_TO_RASTERISER_BIN>```
As an example,

    ```./run_tests -e ../masters/coretext -c ../cases -t ../src/acme.ttf -r ./rasterise_text_coretext```
1. A real‐world example is in the [Sampradaya typeface Makefile](https://github.com/deepestblue/Sampradaya/blob/main/macos/Makefile).

### On Windows

1. `Alekhana` requires Visual Studio Build tools (not the entire IDE, just the command line tools.
1. Launch the x64 Native tools command shell.
1. If you prefer to grab pre‐built binaries, grab the [latest binaries](https://github.com/deepestblue/alekhana/releases/download/latest/alekhana_dist.zip), unzip them and change into the unzipped directory. Skip to Step 9.
1. Grab the [latest zip sources](https://github.com/deepestblue/alekhana/archive/refs/tags/latest.zip).
1. Unzip the sources.
1. `cd alekhana-latest\windows`
1. `nmake dist`
1. `cd ..\dist`
1. The current directory contains the `alekhana` runtime artifacts.
1. The intended workflow is to use the Powershell script `generate_images` to generate a set of master images, and then to use the Powershell script `run_tests` to compare the rendering using the current typeface file against the masters.
1. The test cases need to be organised as text files, all in a flat directory. Each line in each text file is a test string.
1. To generate masters, use `generate_images` as below:

    ```powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ./generate_images.ps1 -OutputRoot <OUTPUT_DIRECTORY_FOR_MASTER_IMAGES> -TestCases <DIRECTORY_OF_TEST_CASES> -TypeFace <PATH_TO_FONT_FILE> -Rasteriser <PATH_TO_RASTERISER_BIN>```
As an example,

    ```powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ./generate_images.ps1 -OutputRoot ../masters/windows -TestCases ../cases -TypeFace ../src/acme.ttf -Rasteriser ./rasterise_text_windows.exe```
1. To run tests, use `run_tests` as below:

    ```powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ./run_tests.ps1 -MasterImages <EXPECTED_MASTER_IMAGES> -TestCases <DIRECTORY_OF_TEST_CASES> -TypeFace <PATH_TO_FONT_FILE> -Rasteriser <PATH_TO_RASTERISER_BIN>```
As an example,

    ```powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ./run_tests.ps1 -MasterImages ../masters/windows -TestCases ../cases -TypeFace ../src/acme.ttf -Rasteriser ./rasterise_text_windows.exe```
1. A real‐world example is in the [Sampradaya typeface Makefile](https://github.com/deepestblue/Sampradaya/blob/main/windows/Makefile).
