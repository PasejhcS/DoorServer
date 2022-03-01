
#include <arduino.h>

const char css_css[] PROGMEM = R"rawliteral(

  .button {
    background: #2fc974;
    border:2px solid #2fc974;
    color: #fff;
    display: inline-block;
    padding: 15px 0px;
    text-align: center;
    text-decoration: none;
    width: 22%;
    border-radius: 8px;
    transition-duration: 0.4s;
  }
  .button:hover{
    background: #83e2ae;
    border:2px solid #83e2ae;
  }
 .openButton {
    background: #fc3000;
    font-family:sans-serif;
    border:2px solid #fc3000;
    color: #fff;
    font-size: 300%;
    display: block;
    margin: 150px auto;
    padding: 45px 10px;
	  text-align: center;
    
    text-decoration: none;
    width: 35%;
    
    border-radius: 8px;
    transition-duration: 0.4s;
  }
    .openButton:hover{
    background: #ff7554;
    border:2px solid #ff7554;
  }

)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Door management</title>
  <link href="/css.css" rel="stylesheet">
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<br>
<a class="button" href="/">Control</a>
<a class="button" href="logs">Logs</a>
<a class="button" href="userHandling">Manage</a>
<a class="button" onclick="logoutButton()">Logout</a>
<br>
<a class="openButton" onclick="openDoor()">Open</a>
<script>
  function logoutButton() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/logout", true);
    xhr.send();
    setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
  }
  function openDoor() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET","/openDoor1234",true);
    xhr.send();
  alert('Door opened!');
  }
</script>
</body>
</html>
)rawliteral";
const char user_handling_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>User handling</title>
  <link href="/css.css" rel="stylesheet">
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<br>
<a class="button" href="/">Control</a>
<a class="button" href="logs">Logs</a>
<a class="button" href="userHandling">Manage</a>
<a class="button" onclick="logoutButton()">Logout</a>
<br>
<form action="/userHandling" method="POST">
    Old password:<input type="password" name="password_old"><br>
    New password:<input type="password" name="password_new1"><br>
    Confirm:<input type="password" name="password_new2"><br>
    <input class="button" type="submit" value="Submit"><br>
    <p style="color:red;">%MESSAGE%</p>

  </form>
</body>
</html>
)rawliteral";
String message_userHandling="";
String processor_userHandling(const String& var)
{
    if(var=="MESSAGE")
    {
        return message_userHandling;
    }
  return "";
}

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
<link href="/css.css" rel="stylesheet">
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a class="button" href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

