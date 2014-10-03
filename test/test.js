var C = require('..');
var fs = require('fs');


fs.readFile('./alphatest.png', function (err, data) {
    if (err) throw err;
    var start = (new Date).getTime();
    var buffer = C.compress(data, {
        speed: 11,
        quality: [50, 60]
    });

    console.log((new Date).getTime() - start);
    start = (new Date).getTime();
    var buffer = C.compress(data, {
        speed: 11,
        quality: [50, 60]
    });

    console.log((new Date).getTime() - start);
    start = (new Date).getTime();
    var buffer = C.compress(data);

    console.log((new Date).getTime() - start);

    fs.writeFile('./alphatest_out.png', buffer, {
        flags: 'wb'
    }, function(err){});
});

