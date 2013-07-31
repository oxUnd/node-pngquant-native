//
//  nearest.h
//  pngquant
//

struct nearest_map;

nearest_map* nearest_init(const colormap* palette);

unsigned int nearest_search(const nearest_map* map, const f_pixel px, const double min_opaque, double* diff);

void nearest_free(nearest_map* map);

