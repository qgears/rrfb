webSocket = new WebSocket("rrfb");
webSocket.binaryType = "arraybuffer";
// webSocket.onmessge = function (ev) {console.info("onmessage "+ev.size);};

webSocket.onopen=(event) => {
  // webSocket.send("Hello Server!");
};
var cursorImage = new Image();
cursorImage.src="../res/cursor.png";

var nframe = 3;
var targetmsPeriod = 100;

pointerScreenX=0;
pointerScreenY=0;
pointerMask=0;
class Reader {
	constructor()
	{
	  this.h=new ArrayBuffer(8);
	  this.hu8=new Uint8Array(this.h);
	  this.headerAt=0;
	  this.nextFrameIndex=0;
	  const queryString = window.location.search;
	  const urlParams = new URLSearchParams(queryString);
	  const showPointer = urlParams.get('showPointer');
	  this.showPointer = showPointer === 'true';
	}
	readu32(uint8, ptr)
	{
		return ((uint8[ptr+3] << 24) | (uint8[ptr+2] << 16) | (uint8[ptr+1] << 8) | uint8[ptr]) >>> 0;
	}
	append(data)
	{
		var at=0;
		var du8=new Uint8Array(data);
		while(at<data.byteLength)
		{
			if(this.headerAt<8)
			{
				// console.log("Header piece from server ", data);
				var remaining=8-this.headerAt;
				var n=Math.min(remaining, data.byteLength-at);
				var slice=du8.slice(at, at+n);
				this.hu8.set(slice, this.headerAt);
				at+=n;
				this.headerAt+=n;
				if(this.headerAt==8)
				{
					this.dataAt=0;
					this.command=this.readu32(this.hu8, 0);
					this.dataLength=this.readu32(this.hu8, 4);
					this.d=new ArrayBuffer(this.dataLength);
					this.du8=new Uint8Array(this.d);
					// console.log("Header piece from server ", this.command, this.dataLength);
				}
			}
			else{
				if(this.dataAt<this.dataLength && at<data.byteLength)
				{
					var remaining=this.dataLength-this.dataAt;
					var n=Math.min(remaining, data.byteLength-at);
					var slice=du8.slice(at, at+n);
					this.du8.set(slice, this.dataAt);
					this.dataAt+=n;
					at+=n;
				}
				if(this.dataAt==this.dataLength)
				{
					// console.info("Command fully received: ",this.command, this.dataLength);
					switch(this.command)
					{
						case 1:
							this.w=this.readu32(this.du8, 0);
							this.h=this.readu32(this.du8, 4);
							console.info("Screen Size: "+this.w+" "+this.h);
							// this.diffImage=new ArrayBuffer(this.w*this.h*4);
							//this.diffImageU8=new Uint8Array(this.diffImage);
							this.image=new ArrayBuffer(this.w*this.h*4);
							this.imageU8=new Uint8Array(this.image);
							var c = document.getElementById("myCanvas");
							c.width=this.w; 
							c.height=this.h; 
							break;
						case 2:
							this.nextFrameIndex++;
							rrfbSendMessage({type: "requestFrame", index: this.nextFrameIndex+nframe, targetmsPeriod: targetmsPeriod});
							const l=this.imageU8.length;
							decodeAdd(this.d, 0, this.dateLength, 4, this.imageU8);
							// var decoded=decode(this.d, 0, this.dateLength, 4);
							// console.info("Decode diff image", decoded, l);
							// this.diffImage=decoded.result;
							// this.diffImageU8=decoded.data;
							//for(var i=0;i<l;++i)
							//{
							//	this.imageU8[i]+=this.diffImageU8[i];
							//}
							// console.info("Diff added! "+l);
							try{
							var c = document.getElementById("myCanvas"); 
    						var ctx = c.getContext("2d");
    						var imageData = ctx.createImageData(this.w, this.h);
    						var idata = imageData.data;                   /// view for the canvas buffer
    						// console.info(idata);
							var len = idata.length;                       /// length of buffer
							// console.info("image data len: "+len);
							var src=this.imageU8;
							for(var i=0; i < len; i +=4) {
							    idata[i]     = src[i+2];     /// copy RGB data to canvas from custom array
							    idata[i+1]     = src[i+1];     /// copy RGB data to canvas from custom array
							    idata[i+2]     = src[i];     /// copy RGB data to canvas from custom array
							    idata[i+3]     = 255; //src[i+3];     /// copy RGB data to canvas from custom array
							}
							ctx.putImageData(imageData, 0, 0);
							if(this.showPointer)
							{
								var size=40;
								ctx.drawImage(cursorImage, pointerScreenX-size*0.162109375, pointerScreenY-size*0.01, size, size);
							} 
							}catch (ex){console.error(ex);}
							
							
							break;
						case 3:
							var string = new TextDecoder().decode(this.du8);
							if(string===currentLocalClipboardContent)
							{
								// Nothing to do
							}else
							{
								if(navigator.clipboard)
								{
									navigator.clipboard.writeText(string);
								}
							}
							console.info("clipboard changed: ",this.command, this.dataLength, string);
							break;
						case 4:
						{
							pointerScreenX=this.readu32(this.du8, 0);
							pointerScreenY=this.readu32(this.du8, 4);
							pointerMask=this.readu32(this.du8, 4);
							break;
						}
						case 5:
						{
							var string = new TextDecoder().decode(this.du8);
							if(string === "RRFB 0.3.0      ")
							{
								console.info("Correct RRFB version string received: "+string);
							}else
							{
								console.error("Incorrect RRFB version string received: '"+string+"' try best effort");
							}
							break;
						}
						default:
							console.info("unhandled command: ",this.command, this.dataLength);
					}
					this.headerAt=0;
				}
			}
		}
	}
}

r=new Reader();
// Listen for messages
webSocket.onmessage=(event) => {
  r.append(event.data);
}; 

/*
window.addEventListener("load",()=>{
	var charfield = document.getElementById("bodyId");
	charfield.onkeypress = function(e) {
	console.info(e);
	    e = e || window.event;
	    var charCode = (typeof e.which == "number") ? e.which : e.keyCode;
	    if (charCode > 0) {
	        console.info("Typed character: " + String.fromCharCode(charCode));
	    }
	};
});
*/

globalDisableMouse=false;

updateNframeView = function()
{
	 var fm=document.getElementById("nframe");
	 fm.innerHTML = ""+nframe;
	 fm=document.getElementById("targetms");
	 fm.innerHTML = ""+targetmsPeriod;
}


window.addEventListener("load",()=>{
	 var fm=document.getElementById("floatmenu");
	 document.getElementById("floatmenu_hide").addEventListener("click", e=>{
	  globalDisableMouse=true;
	  setTimeout(()=>{
	     globalDisableMouse=false;
	     document.getElementById("floatmenu").style.display='none';
	  	}, 1000);
	  });
	 document.getElementById("nframep").addEventListener("click", e=>{
	   nframe++;
	   updateNframeView();
	  });
	 document.getElementById("nframem").addEventListener("click", e=>{
	   nframe--;
	   updateNframeView();
	  });
	 document.getElementById("targetmsp").addEventListener("click", e=>{
	   targetmsPeriod+=10;
	   updateNframeView();
	  });
	 document.getElementById("targetmsm").addEventListener("click", e=>{
	   targetmsPeriod-=10;
	   updateNframeView();
	  });
	fm.style.display='none';
	fm.addEventListener("mousemove", e=>{
	    e.stopPropagation();e.preventDefault();
	});
	fm.addEventListener("mousedown", e=>{
	    e.stopPropagation();e.preventDefault();
	});
	fm.addEventListener("mouseup", e=>{
	    e.stopPropagation();e.preventDefault();
	});
    updateNframeView();
  });


rrfbSendInputEvent=function(e, msg)
{
	if(!globalDisableMouse)
	{
		rrfbSendMessage(msg);
	}
	// console.info(e);
	e.stopPropagation();e.preventDefault();
}
rrfbSendMessage=function(msg)
{
	msg.t=Date.now();
	if(webSocket.readyState<=1)
	{
		webSocket.send(JSON.stringify(msg));
	}
}

localProcessKeyEvent=function(type, e)
{
	if(e.key==='F11')
	{
		return true;
	}
	if(e.key==='F1')
	{
		document.getElementById("floatmenu").style.display='block';
	    e.stopPropagation();e.preventDefault();
		return true;
	}
	return false;
}
document.addEventListener("keydown", e=>{
	if(!localProcessKeyEvent("keydown", e))
	{
		console.info("keydown-");
		console.info(e.keyCode);
		console.info(e);
	    rrfbSendInputEvent(e, {type: "keydown", code:e.code, key: e.key, keyCode: e.keyCode});
	}
	});
document.addEventListener("keyup", e=>{
	if(!localProcessKeyEvent("keyup", e))
	{
		console.info("keyup");
		console.info(e);
	    rrfbSendInputEvent(e, {type: "keyup", code:e.code, key: e.key, keyCode: e.keyCode});
//		e.stopPropagation();e.preventDefault();
	}
	});

document.addEventListener("keypressed", e=>{
	if(!localProcessKeyEvent("keypressed", e))
	{
		// 
		e.stopPropagation();e.preventDefault();
	}
	});
document.addEventListener("mousemove", e=>{
	rrfbSendInputEvent(e, {type: "mousemove", x: e.clientX, y: e.clientY});
	});
document.addEventListener("mousedown", e=>{
	rrfbSendInputEvent(e, {type: "mousedown", x: e.clientX, y: e.clientY, button:e.button});
});
document.addEventListener("mouseup", e=>{
	rrfbSendInputEvent(e, {type: "mouseup", x: e.clientX, y: e.clientY, button:e.button});
});
document.addEventListener("click", e=>{console.info(e);e.stopPropagation();e.preventDefault();});
document.addEventListener("dblclick", e=>{console.info(e);e.stopPropagation();e.preventDefault();});
document.addEventListener('contextmenu', event => event.preventDefault());
document.addEventListener("wheel", e => {
	allDy+=e.deltaY;
	var limit=100.0;
	while(allDy>limit)
	{
		console.info(allDy);
		rrfbSendInputEvent(e, {type: "wheel", deltaX: e.deltaX, deltaY: 1});
		allDy-=limit;
	}
	while(allDy<-limit)
	{
		console.info(allDy);
		rrfbSendInputEvent(e, {type: "wheel", deltaX: e.deltaX, deltaY: -1});
		allDy+=limit;
	}
// rrfbSendInputEvent(e, {type: "wheel", deltaX: e.deltaX, deltaY: e.deltaY});
	e.stopPropagation();e.preventDefault();});

//addEventListener("scroll", (event) => {console.info("scroll");});

currentLocalClipboardContent="";
allDy=0;
triggerListenClipboard=function()
{
	if(navigator.clipboard)
	{
	navigator.clipboard.readText()
	  .then(text => {
	    var newValue=text;
	    if(currentLocalClipboardContent===newValue)
	    {
	    // nothing to do
	    }else
	    {
	    console.log('Clipboard content changed: ', text);
	    currentLocalClipboardContent=text;
	    	msg={type: "clipboard", text: text};
	    	msg.t=Date.now();
			// if(!globalDisableMouse)
			{
				if(webSocket.readyState<=1)
				{
					webSocket.send(JSON.stringify(msg));
				}
			}
	    }
	    setTimeout(triggerListenClipboard, 500);
	  })
	  .catch(err => {
	    // console.error('Failed to read clipboard contents: ', err);
	    setTimeout(triggerListenClipboard, 500);
	  });
	}
//	navigator.clipboard.addEventListener("copy", e=>{console.info(e);});
//	navigator.clipboard.addEventListener("clipboardchange", e=>{console.info(e);});
	
}

triggerListenClipboard();

