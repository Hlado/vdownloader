# Setup

Run bootstrapping script:

    cd <vdownloader_dir>
    ./bootstrap.sh

Set desired target triplet variable value to CMakePresets.json:

    {
        ...
        "configurePresets": [
            ...
            "cacheVariables": {
                ...
                "VCPKG_TARGET_TRIPLET": "<triplet_name>"
            }
        ]
    }
    