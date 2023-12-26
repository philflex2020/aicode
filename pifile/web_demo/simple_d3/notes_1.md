To load the menu items from an external file and specify their background colors, you'll need to use JavaScript to fetch the data from the file and then dynamically create the menu items in your HTML. You can use a JSON file for the external data, which is a common format for such tasks.

### Step 1: Create a JSON File

Create a JSON file (`menu-items.json`) with the necessary data. For example:

```json
[
    {
        "name": "Ess",
        "color": "#f0f0f0"
    },
    {
        "name": "Pcs",
        "color": "#add8e6"
    },
    {
        "name": "Bms",
        "color": "#90ee90"
    }
]
```

### Step 2: Fetch and Load the Data

Use JavaScript to fetch the data from this JSON file and create the menu items dynamically. Here's an example using `fetch` API:

```javascript
document.addEventListener("DOMContentLoaded", function() {
    fetch('menu-items.json')
        .then(response => response.json())
        .then(data => {
            createPopupMenu(data);
        })
        .catch(error => console.error('Error:', error));
});

function createPopupMenu(items) {
    const popupMenu = d3.select("#popupMenu");
    items.forEach(item => {
        popupMenu.append("div")
            .attr("class", "menu-item " + item.name.toLowerCase() + "-item")
            .attr("data-icon", item.name)
            .style("background-color", item.color)
            .text(item.name);
    });
}
```

### Explanation

- The `fetch` function is used to retrieve the data from the `menu-items.json` file.
- The `createPopupMenu` function takes the fetched data and dynamically creates menu items based on it.
- Each menu item is assigned a class based on its name and given the specified background color.

### HTML and CSS Adjustments

Make sure your HTML has the container for the popup menu:

```html
<div id="popupMenu" class="popup-menu" style="display:none;"></div>
```

Your existing CSS should still apply, but ensure it's set up to style `.menu-item` appropriately.

### Serving the JSON File

Make sure your JSON file is placed in a location that's accessible by your web page, such as in the same directory. Depending on how you serve your web content, you might need to ensure that the server is configured to serve JSON files.

### CORS Policy

If you're fetching the file from a different domain or port, be aware of Cross-Origin Resource Sharing (CORS) policies. For local testing, you may need to serve your HTML and JSON through a local server rather than opening the HTML file directly in a browser. 

This approach will create a more dynamic and configurable menu, allowing easy changes to the menu items without altering your HTML directly.
To create curvy lines instead of straight ones between your elements, you can use SVG paths with a Bezier curve. SVG paths allow you to define complex shapes including curved lines. A common type of curve used for such purposes is the Cubic Bezier curve, defined with the `C` command in SVG path data.

Here’s how you can modify your code to draw curvy lines:

### Update the Line Drawing Logic

Replace the line creation with an SVG path:

```javascript
if (selectedElement !== element) {
    const x1 = parseInt(selectedElement.style("left")) + 50;
    const y1 = parseInt(selectedElement.style("top")) + 50;
    const x2 = parseInt(element.style("left")) + 50;
    const y2 = parseInt(element.style("top")) + 50;

    // Define control points for the Bezier curve
    const ctrlX1 = (x1 + x2) / 2;
    const ctrlY1 = y1;
    const ctrlX2 = (x1 + x2) / 2;
    const ctrlY2 = y2;

    // Create the SVG path for the Bezier curve
    const path = svg.append("path")
        .attr("d", `M ${x1} ${y1} C ${ctrlX1} ${ctrlY1}, ${ctrlX2} ${ctrlY2}, ${x2} ${y2}`)
        .attr("class", "line");

    console.log("Drawing curved line:", x1, y1, x2, y2);
    lines.push({ path, start: selectedElement, end: element });

    // ... rest of your code ...
}
```

### Explanation

- `M ${x1} ${y1}` moves the pen to the starting point `(x1, y1)`.
- `C ${ctrlX1} ${ctrlY1}, ${ctrlX2} ${ctrlY2}, ${x2} ${y2}` draws a Cubic Bezier curve from the current point to `(x2, y2)` using `(ctrlX1, ctrlY1)` and `(ctrlX2, ctrlY2)` as control points.
- The control points here are chosen to be in the middle between `x1` and `x2`, vertically aligned with the start and end points. You can adjust these to change the curve's shape.
- The `path` element is used instead of `line` to allow for the curved path.

### Update the CSS for Path (if needed)

If you have specific styles for lines, ensure they are applied to paths as well:

```css
.line, path {
    stroke: black;
    stroke-width: 2px;
    fill: none; /* Important for paths */
}
```

With these changes, your lines between elements will be curved, providing a visually distinct and potentially more appealing connection representation. You can further tweak the control points of the Bezier curve to adjust the curvature to your liking.
To adjust your `dragged` function for use with a Bezier curve path in D3.js, you need to modify the way the lines (paths) are updated. Instead of directly setting `x1`, `y1`, `x2`, `y2` attributes (as used for straight lines), you'll define a Bezier curve path that updates based on the positions of the start and end nodes.

Here's an approach to adjust the `dragged` function:

### Step 1: Define a Bezier Curve Path Function

First, define a function that calculates the path for a Bezier curve given the start and end points. A simple cubic Bezier curve can be used:

```javascript
function bezierPath(startX, startY, endX, endY) {
    var controlX1 = (startX + endX) / 2;
    var controlY1 = startY;
    var controlX2 = (startX + endX) / 2;
    var controlY2 = endY;
    return "M" + startX + "," + startY +
           "C" + controlX1 + "," + controlY1 +
           " " + controlX2 + "," + controlY2 +
           " " + endX + "," + endY;
}
```

### Step 2: Update the `dragged` Function

Modify the `dragged` function to use the `bezierPath` function for updating paths:

```javascript
function dragged(event, d) {
    isDragged = true; 
    d3.select(this).style("left", event.x + "px").style("top", event.y + "px");

    lines.forEach(function(l) {
        if (l.start.node() === this || l.end.node() === this) {
            var startX = parseInt(l.start.style("left")) + 50;
            var startY = parseInt(l.start.style("top")) + 50;
            var endX = parseInt(l.end.style("left")) + 50;
            var endY = parseInt(l.end.style("top")) + 50;
            l.line.attr("d", bezierPath(startX, startY, endX, endY));
        }
    }, this);
}
```

### Explanation:

- `bezierPath` function creates a path string for a cubic Bezier curve. The control points are chosen to be in the middle between the start and end points, which creates a smooth curve. You can adjust the control points to change the curve's shape.
- Inside the `dragged` function, for each line connected to the dragged element, the start and end coordinates are calculated, and the path of the line is updated using the `bezierPath` function.

This setup will create a dynamic Bezier curve that updates as you drag the nodes. The control points for the Bezier curve are set to create a simple curve, but you can modify them to achieve different styles of curvatures.