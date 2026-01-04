/**
 * clasp.js - Plugin-side JavaScript library for clasp-gui
 * Include this in your plugin's HTML to communicate with C++ via clasp::Protocol
 */
(function() {
    'use strict';

    // Event handlers registry
    var handlers = {};

    // Call ID counter for request/response correlation
    var callId = 0;
    var pendingCalls = {};

    // Drag state
    var dragState = {
        active: false,
        onMove: null,
        onEnd: null
    };

    // The clasp object
    var clasp = {
        /**
         * Subscribe to an event from C++
         * Events: paramChange, paramsSync, noteOn, noteOff, midiCC, ready
         */
        on: function(event, handler) {
            if (!handlers[event]) {
                handlers[event] = [];
            }
            handlers[event].push(handler);
        },

        /**
         * Remove an event handler
         */
        off: function(event, handler) {
            if (!handlers[event]) return;
            var idx = handlers[event].indexOf(handler);
            if (idx !== -1) {
                handlers[event].splice(idx, 1);
            }
        },

        /**
         * Call a C++ function registered via Protocol::onCall()
         * Returns a Promise that resolves with the result
         */
        call: function(name) {
            var args = Array.prototype.slice.call(arguments, 1);
            var id = ++callId;

            return new Promise(function(resolve, reject) {
                pendingCalls[id] = { resolve: resolve, reject: reject };

                var msg = JSON.stringify({
                    t: 'call',
                    fn: name,
                    args: args,
                    id: id
                });

                // Send via the __clasp binding
                if (typeof __clasp === 'function') {
                    __clasp(msg);
                } else {
                    reject(new Error('clasp: __clasp binding not available'));
                    delete pendingCalls[id];
                }
            });
        },

        /**
         * Send a message to C++ (fire-and-forget)
         */
        send: function(type, payload) {
            var msg = JSON.stringify({
                t: 'msg',
                type: type,
                payload: payload
            });

            if (typeof __clasp === 'function') {
                __clasp(msg);
            }
        },

        /**
         * Start a drag operation (avoids pointer capture banner)
         * @param {function} onMove - Called with (x, y, dx, dy) on mouse move
         * @param {function} onEnd - Called when drag ends
         */
        startDrag: function(onMove, onEnd) {
            dragState.active = true;
            dragState.onMove = onMove;
            dragState.onEnd = onEnd;
            document.body.style.cursor = 'grabbing';
            document.body.style.userSelect = 'none';
        },

        /**
         * End the current drag operation
         */
        endDrag: function() {
            if (dragState.active) {
                if (dragState.onEnd) {
                    dragState.onEnd();
                }
                dragState.active = false;
                dragState.onMove = null;
                dragState.onEnd = null;
                document.body.style.cursor = '';
                document.body.style.userSelect = '';
            }
        },

        /**
         * Disable the browser context menu
         */
        disableContextMenu: function() {
            document.addEventListener('contextmenu', function(e) {
                e.preventDefault();
            });
        }
    };

    // Internal: emit an event to handlers
    function emit(event, args) {
        if (!handlers[event]) return;
        for (var i = 0; i < handlers[event].length; i++) {
            try {
                handlers[event][i].apply(null, args);
            } catch (e) {
                console.error('clasp: error in', event, 'handler:', e);
            }
        }
    }

    // Internal: receive messages from C++
    // C++ calls: __clasp_recv(jsonString)
    window.__clasp_recv = function(json) {
        var msg;
        try {
            msg = JSON.parse(json);
        } catch (e) {
            console.error('clasp: invalid message:', json);
            return;
        }

        switch (msg.t) {
            case 'param':
                emit('paramChange', [msg.id, msg.v]);
                break;

            case 'params':
                emit('paramsSync', [msg.params]);
                // Also emit individual paramChange for each
                if (msg.params) {
                    for (var i = 0; i < msg.params.length; i++) {
                        var p = msg.params[i];
                        emit('paramChange', [p.id, p.v]);
                    }
                }
                break;

            case 'noteOn':
                emit('noteOn', [msg.ch, msg.k, msg.vel]);
                break;

            case 'noteOff':
                emit('noteOff', [msg.ch, msg.k]);
                break;

            case 'midiCC':
                emit('midiCC', [msg.ch, msg.cc, msg.v]);
                break;

            case 'ready':
                emit('ready', []);
                break;

            case 'reply':
                // Response to a call()
                if (pendingCalls[msg.id]) {
                    if (msg.error) {
                        pendingCalls[msg.id].reject(new Error(msg.error));
                    } else {
                        pendingCalls[msg.id].resolve(msg.result);
                    }
                    delete pendingCalls[msg.id];
                }
                break;

            default:
                // Unknown message type - emit as generic event
                emit(msg.t, [msg]);
        }
    };

    // Mouse event handlers for drag
    document.addEventListener('mousemove', function(e) {
        if (dragState.active && dragState.onMove) {
            dragState.onMove(e.clientX, e.clientY, e.movementX, e.movementY);
        }
    });

    document.addEventListener('mouseup', function(e) {
        clasp.endDrag();
    });

    // Expose globally
    window.clasp = clasp;

    // Signal ready after DOM loaded
    if (document.readyState === 'complete' || document.readyState === 'interactive') {
        setTimeout(function() { emit('ready', []); }, 0);
    } else {
        document.addEventListener('DOMContentLoaded', function() {
            emit('ready', []);
        });
    }

})();
