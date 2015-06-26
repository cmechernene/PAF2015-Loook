

var static = require('node-static');

var file = new static.Server('./public', {headers:{'Access-Control-Allow-Origin':'*'}, cache:0});

require('http').createServer(function (request, response) {
    file.serve(request, response, function (err, res) {
        if (err) { // An error as occured
            console.error("> Error serving " + request.url + " - " + err.message);
            response.writeHead(err.status, err.headers);
            response.end();
        } else { // The file was served successfully
            console.log("> " + request.url + " - " + res.message);
        }
    });
}).listen(8080, '0.0.0.0');

// console.log("> node-static is listening on http://127.0.0.1:8080");
