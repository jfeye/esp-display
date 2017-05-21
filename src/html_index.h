const char html_index[] PROGMEM = R"=====(
  <!DOCTYPE HTML>
  <html>
  <head lang="de">
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED Display WiFi Settings</title>
    <script>
      function gebID(e) {
        return document.getElementById(e);
      }
      function req(url) {
        var xhr = new XMLHttpRequest();
        var res = "";
        xhr.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            res = this.responseText;
            //console.log(res);
          }
        };
        xhr.open("GET", url, false); // true for asynchronous
        xhr.send(null);
        return res;
      }
      var scans = [];
      var stored = [];

      function getScans() {
        scans = JSON.parse(req("get?v=s"));
        var theTable = gebID("scans");
        var newRow;
        var newCell;
        for (i = 0; i < scans.length; i++) {
          newRow = theTable.insertRow(theTable.rows.length);
          newCell = newRow.insertCell(0);
          newCell.onclick = function() {
            gebID("ssid").defaultValue = this.innerHTML;
          };
          newCell.innerHTML = scans[i];
        }
        theTable.getElementsByTagName("tbody")[0].align = "left";

        stored = JSON.parse(req("get?v=e"));
        theTable = gebID("stored");
        for (i = 0; i < stored.length; i++) {
          newRow = theTable.insertRow(theTable.rows.length);
          newCell = newRow.insertCell(0);
          newCell.onclick = function() {
            gebID("ssid").defaultValue = this.innerHTML;
          };
          newCell.innerHTML = stored[i];
        }
        theTable.getElementsByTagName("tbody")[0].align = "left";
      }
    </script>
    <style>
      * {
        font-family: sans-serif;
        font-size: 18pt;
        color: #000;
      }
      button {
        width: 80px;
        margin: 2px 2px -10px 2px;
      }
      div {
        text-align: center;
        margin-bottom: 10px;
      }
      legend {
        padding-left: 10px;
        padding-right: 10px;
      }
      input[type=text] {
        display: block;
        margin: auto;
        width: 96%;
      }
      input[type=password] {
        display: block;
        margin: auto;
        width: 96%;
      }
    </style>
  </head>
  <body onload="getScans();">
    <p align="center">
      <div>
        <fieldset>
          <legend>WiFis in range</legend>
          <table id="scans" align="center" cellpadding="5">
          </table>
        </fieldset>
      </div>
      <div>
        <fieldset>
          <legend align="center">Enter WiFi data</legend>
          <table>
            <tr>
              <td>SSID:</td>
              <td><input id="ssid" type="text" size=128 onchange="gebID('store').disabled=false"></td>
            </tr>
            <tr>
              <td>Key:</td>
              <td><input id="key" type="password" size=128 onchange="gebID('store').disabled=false"></td>
            </tr>
          </table>
          <button id="store" disabled onclick="this.disabled=true">Store</button>
        </fieldset>
      </div>
      <div>
        <fieldset>
          <legend>Stored WiFis</legend>
          <table id="stored">
          </table>
        </fieldset>
      </div>
    </p>
  </body>
  </html>
)=====";
