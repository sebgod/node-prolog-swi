{
    "targets": [
    ],
    "conditions": [
        ["OS!='win'", {
            "targets": [
                {
                    "cflags_cc": [ "-std=c++11", "<!@(pkg-config swipl --cflags-only-other)" ],
                    "type": "<(library)",
                    "target_name": "libswipl",
                    "product_prefix": "lib",
                    "sources": [
                        "./src/libswipl.cc"
                    ],
                    "include_dirs": [
                        "./src",
                        "<!@(pkg-config swipl --variable=PLBASE)/include"
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
