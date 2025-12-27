// This is a JS hook that allows you to hack the Liferea rendering
//
// It is invoked by the htmlview.js rendering scripting and provides you
// with hooks one to modify the data before rendering and one to modify
// or enhance the rendering after it is done.

// hooks to modify item info rendering

window.hookPreItemRendering = (data) => {
        return data;
};
window.hookPostItemRendering = () => {
        // Modify DOM as you like
};

// hooks to modify node info rendering

window.hookPreNodeRendering = (data) => {
        return data;

};
window.hookPostNodeRendering = () => {
        // Modify DOM as you like
};