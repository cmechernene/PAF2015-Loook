var express = require('express');
var serveStatic = require('serve-static');

var app = express();
app.use(serveStatic('public', {'index':['index.html'], 'setHeaders' : setHeaders}));

function setHeaders(res){
	res.setHeader('Access-Control-Allow-Origin', '*');
}

app.listen(8080, '0.0.0.0');
