{
    'conditions': [
        ['OS=="win"', {
            'variables': {
                'THIRD_PATH%': 'D:/dev/node/node-pngquant-native/third/'
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
        'conditions': [
            ['OS == "win"', {
                'libraries': [
                    '-l<(THIRD_PATH)/libpng/projects/vstudio/ReleaseLibrary/libpng15.lib',
                    '-l<(THIRD_PATH)/libpng/projects/vstudio/ReleaseLibrary/zlib.lib'],
                'include_dirs': [
                    '<(THIRD_PATH)/libpng',
                    '<(THIRD_PATH)/zlib']
            }, {
                'libraries': [
                    '-lpng',
                    '-lz',
                    '-lm'
                ]
            }]
        ],
    }]
}
