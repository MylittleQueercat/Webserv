document.addEventListener("DOMContentLoaded", function () {
    var message = document.getElementById("js-message");
    var clock = document.getElementById("clock");

    message.textContent = "JavaScript loaded successfully from script.js 🎉";

    function updateClock() {
        var now = new Date();
        clock.textContent = "Current local time: " + now.toLocaleString();
    }

    updateClock();
    setInterval(updateClock, 1000);
});