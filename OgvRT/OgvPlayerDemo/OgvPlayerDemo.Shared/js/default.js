// For an introduction to the Blank template, see the following documentation:
// http://go.microsoft.com/fwlink/?LinkID=392286
(function () {
    "use strict";

    var app = WinJS.Application;
    var activation = Windows.ApplicationModel.Activation;
    var mediaExtensions;

    function initMediaExtensions() {
        if (!mediaExtensions) {
            mediaExtensions = new Windows.Media.MediaExtensionManager();
            mediaExtensions.registerByteStreamHandler("OgvSource.OgvByteStreamHandler", ".ogv", "video/ogg");
        }
    }

    app.onactivated = function (args) {
        if (args.detail.kind === activation.ActivationKind.launch) {
            if (args.detail.previousExecutionState !== activation.ApplicationExecutionState.terminated) {
                // TODO: This application has been newly launched. Initialize
                // your application here.
            } else {
                // TODO: This application has been reactivated from suspension.
                // Restore application state here.
            }

            initMediaExtensions();

            var player = document.getElementById('player');
            document.getElementById('play-mp4').addEventListener('click', function () {
                player.src = "/media/sharks.mp4";
                player.load();
            });
            document.getElementById('play-ogv').addEventListener('click', function () {
                player.src = "/media/sharks.ogv";
                player.load();
            });
            args.setPromise(WinJS.UI.processAll());
        }
    };

    app.oncheckpoint = function (args) {
        // TODO: This application is about to be suspended. Save any state
        // that needs to persist across suspensions here. You might use the
        // WinJS.Application.sessionState object, which is automatically
        // saved and restored across suspension. If you need to complete an
        // asynchronous operation before your application is suspended, call
        // args.setPromise().
    };

    app.start();
})();