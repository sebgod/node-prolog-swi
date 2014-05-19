{
    "targets": [
    ],
    "conditions": [
        ["OS!='win'", {
            "targets": [
                {
                    "cflags_cc": [ "<!@(pkg-config swipl --cflags-only-other)" ],
                    #"type": "<(library)",
                    "target_name": "libswipl",
                    "product_prefix": "lib",
                    "sources": [
                        "./src/libswipl.cc"
                    ],
                    "include_dirs": [
                        "./src",
                        "<!@(pkg-config swipl --cflags-only-I | sed s/-I//g)"
                    ],
                    "direct_dependent_settings": {
                        "linkflags": [
                            "-D_FILE_OFFSET_BITS=64",
                            "-D_LARGEFILE_SOURCE"
                        ]
                    },
                    "libraries": [
                        "<!@(pkg-config swipl --libs)"
                    ]
                }
            ]
        }]
    ]
}
