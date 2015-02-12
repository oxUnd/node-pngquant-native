{
    'conditions': [
        ['OS=="win"', {
            'variables': {
                'THIRD_PATH%': 'D:/addon/node-pngquant-native/third/'
            }
        }]
    ],
    'targets': [{

        'target_name': 'pngquant_native',
        'cflags': [
            '-g',
            '-DNO_ALONE',
            '-DDEBUG',
            '-fno-inline',
            '-O0',
            '-fstrict-aliasing', 
            '-ffast-math',
            '-funroll-loops', 
            '-fomit-frame-pointer', 
            '-ffinite-math-only',
            '-std=c99'

        ],
        'sources': [
            'src/pngquant_native.cpp',
            'src/pngquant/pngquant.cpp',
            'src/pngquant/getopt.c',
            'src/pngquant/rwpng.cpp',
            'src/pngquant/pam.cpp',
            'src/pngquant/mediancut.cpp',
            'src/pngquant/blur.cpp',
            'src/pngquant/mempool.cpp',
            'src/pngquant/viter.cpp',
            'src/pngquant/nearest.cpp', 
        ],
        'include_dirs': [
            "<!(node -e \"require('nan')\")"
        ],
        'dependencies': [
            './gyp/gyp/libpng.gyp:libpng',
            './gyp/gyp/zlib.gyp:zlib'
        ],
        'conditions': [
            ['OS == "win"', {
            }, {
                'libraries': [
                    '-lm'
                ]
            }]
        ],
    }]
}
