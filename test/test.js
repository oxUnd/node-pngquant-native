var C = require('..');
var fs = require('fs');


fs.readFile('./alphatest.png', function (err, data) {
    if (err) throw err;
    var buffer = C.compress(data, {
        speed: 11,
        quality: [50, 60]
    });

    fs.writeFile('./alphatest_out.png', buffer, {
        flags: 'wb'
    }, function(err){});
});

