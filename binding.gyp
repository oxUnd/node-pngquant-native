{
    'conditions': [
        ['OS=='win'', {
            'variables': {
                'THIRD_PATH%': './third'
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
            'src/pngquant/pngquant.c',
            'src/pngquant/rwpng.c',
            'src/pngquant/lib/pam.c',
            'src/pngquant/lib/mediancut.c',
            'src/pngquant/lib/blur.c',
            'src/pngquant/lib/mempool.c',
            'src/pngquant/lib/viter.c',
            'src/pngquant/lib/nearest.c', 
            'src/pngquant/lib/libimagequant.c',
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
