
var ArduinoReader = null;       // Namespace voor de arduino reader
var DataPlotter = null;         // Namespace voor de data plotter

// Dit is de main class voor de reader
function arduinoReader(autostart){
	var Me = this;
	
	var address      = "http://10.0.1.3",   // adres van de arduino
		timeout      = 1000,                // de duur tussen de calls
		running      = false,               // loopt de reader
		busy         = false,               // wacht de reader op een antwoord
		bufferLength = 100;                 // lengte van de buffer array
	
	Me.Buffer        = [];                  // buffer om de voorgaande stappen op te slaan
		
	
	function construct(autostart){
		if(autostart)Me.Start();
		if(autostart)Me.Start();
		
		for(var i=0; i<bufferLength; i++){
			Me.Buffer[i]=null;
		}
	}
	
	Me.Start = function(){
		if(running)return console.error("reader already running");
		console.log("starting reader");
		running = true;
		window.setTimeout(get_data, timeout);
	}
	
	Me.Stop = function(){
		if(!running)return console.error("reader is not running");
		console.log("stopping reader");
		running = false;
	}
	
	Me.SetAddress = function(elem){
		address = elem.value;
	}
	
	function get_data(){
		if(running)window.setTimeout(get_data, timeout);
		if(busy){return console.error("reader still busy");Me.Buffer.push(null)};
		busy = true;
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function(){
			if(xhttp.readyState==4){
				busy = false;
				if(running)handle_response(xhttp);
			}
		}
		xhttp.open("POST", address, true);
		xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
		xhttp.send();
	}
	
	function handle_response(xhttp){
		if(xhttp.status!=200)return console.error("connection error: "+xhttp.status);
		Me.LastResponse = JSON.parse(xhttp.responseText);
		Me.Buffer.push(JSON.parse(xhttp.responseText));
		while(Me.Buffer.length>bufferLength){Me.Buffer.shift()}
		DataPlotter.PlotData(Me.Buffer);
	}
	
	construct.apply(this, arguments);
}

function dataPlotter(){
	var Me = this;
	
	var canvas = null,
		lastdata = null;
	
	function construct(){
		canvas = document.getElementById("data_plotter");
		resize_canvas();
		
		window.addEventListener("resize", function(){
			resize_canvas();
		})
	}
	
	function resize_canvas(){
		var w = window.innerWidth-20;
		var h = window.innerHeight-370;
		if(h>400)h=400;
		canvas.width = w;
		canvas.height = 400;
	}
	
	Me.PlotData = function(data){
		var ctx = canvas.getContext("2d");
		ctx.fillStyle="#ffffff";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		ctx.strokeStyle = "#ff0000";
		drawForKey(ctx, data, "water_t");
		ctx.strokeStyle = "#ffff00";
		drawForKey(ctx, data, "dht0_h");
		ctx.strokeStyle = "#00ff00";
		drawForKey(ctx, data, "dht0_t");
		ctx.strokeStyle = "#00ffff";
		drawForKey(ctx, data, "dht1_h");
		ctx.strokeStyle = "#0000ff";
		drawForKey(ctx, data, "dht1_t");
	}
	
	function drawForKey(ctx, data, key){
		var dotspace = canvas.width/(data.length-1);
		var fdone = false;
		ctx.beginPath();
		for(var i=0; i<data.length; i++){
			if(data[i]!=null){
				if(!fdone){fdone=true; ctx.moveTo(i*dotspace, canvas.height-data[i][key]*4)}
				else{ctx.lineTo(i*dotspace, canvas.height-data[i][key]*4)}
			}
		}
		ctx.stroke();
	}
	
	construct.apply(this, arguments);
}


window.addEventListener("load", function(){
	ArduinoReader = new arduinoReader();
	DataPlotter = new dataPlotter();
});
