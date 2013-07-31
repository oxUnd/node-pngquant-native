{
    'conditions': [
        ['OS=="win"', {
            'variables': {
                'THIRD_PATH%': 'D:/dev/node/marred-pngquant-master/third/'
            }
        }]
    ],
    'targets': [{

        'target_name': 'pngquant_native',
        'type': 'executable',
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
            'pngquant.cpp',
            'getopt.c',
            'rwpng.cpp',
            'pam.cpp',
            'mediancut.cpp',
            'blur.cpp',
            'mempool.cpp',
            'viter.cpp',
            'nearest.cpp', 
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
