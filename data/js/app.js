// used when hosting the site on the ESP8266
// var address = location.hostname;
var urlBase = "";

// used when hosting the site somewhere other than the ESP8266 (handy for testing without waiting forever to upload to SPIFFS)
// var address = "esp8266-1920f7.local";
// var urlBase = "http://" + address + "/";

let power = 0;
let color = null;

$(document).ready(function() {
  $("#status").html("Connecting, please wait...");

  $.get(urlBase + "all", function(data) {
      $("#status").html("Loading, please wait...");

      setupPowerField(data.power);
      updateColor(data.color);

      $("#form").show();
      $("#status").html("Ready");
    })
    .fail(function(errorThrown) {
      console.log("error:", errorThrown);
      $("#status").html("Failed");
    });
});

function setupPowerField(value) {
  let btnOn = $("#btnOn");
  let btnOff = $("#btnOff");
  power = value;
  setBtnClasses(power, btnOn, btnOff);

  btnOn.click(function() {
    if(!power) setPower(1, btnOn, btnOff);
  });
  btnOff.click(function() {
    if(power) setPower(0, btnOn, btnOff);
  });
}

function setPower(value, btnOn, btnOff) {
  setBtnClasses(value, btnOn, btnOff);

  postValue('power', {value})
    .then( () => power = value )
    .catch( () => setBtnClasses(power, btnOn, btnOff) );
}

function setBtnClasses(value, btnOn, btnOff) {
  btnOn.attr("class", value ? "btn btn-primary" : "btn btn-default");
  btnOff.attr("class", !value ? "btn btn-primary" : "btn btn-default");
}

function postValue(name, body) {
  $("#status").html("Setting " + name + ", please wait...");

  return new Promise((resolve, reject) => {
    $.post(urlBase + name, body)
      .done(data => {
        $("#status").html("Set " + name + ": " + JSON.stringify(data));
        resolve(data);
      })
      .fail(error => {
        $("#status").html("Failed setting " + name);
        console.log("Error", error);
        reject(error);
      });
    }
  );
}

function updateColor(value) {
  let colorField = $("#color .jscolor")[0].jscolor;
  color = value;
  let hsv = {
    h: color.h/255 * 360,
    s: color.s/255 * 100,
    v: color.v/255 * 100
  }
  colorField.fromHSV(hsv.h, hsv.s, hsv.v);
}

function setColor(value) {
    
  postValue('color', value)
    .then( () => updateColor(value) )
    .catch( () => updateColor(color) );
}

function onColor(colorField) {
  let hsv = {
    h: colorField.hsv[0],
    s: colorField.hsv[1],
    v: colorField.hsv[2]
  };
  let conv = {
    h: Math.round(hsv.h/360 * 255),
    s: Math.round(hsv.s/100 * 255),
    v: Math.round(hsv.v/100 * 255)
  }
  
  setColor(conv);
}
