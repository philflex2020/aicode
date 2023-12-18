
const svg = d3.select("body").append("svg")
.attr("width", window.innerWidth)
.attr("height", window.innerHeight)
.style("position", "absolute")
.style("top", 0)
.style("left", 0);


document.addEventListener("DOMContentLoaded", function() {
    let selectedElement = null;
    const lines = [];
    const conns = [];
    let isDragged = false; 

    const svg = d3.select("body").append("svg")
        .attr("width", window.innerWidth)
        .attr("height", window.innerHeight)
        .style("position", "absolute")
        .style("top", 0)
        .style("left", 0);

    const drag = d3.drag()
        .on("start", dragstarted)
        .on("drag", dragged)
        .on("end", dragended);
    
    // Apply the drag behavior to existing icons
    //applyDragBehavior(d3.selectAll('.icon'));

    let menuPosition = { x: 0, y: 0 };

    // Function to apply the drag behavior
    function applyDragBehavior(selection) {
        selection.call(drag);
    }
    // Initialize behavior for existing and future icons
    function initializeIconBehavior() {
        // Apply drag behavior
        applyDragBehavior(d3.selectAll('.icon'));

        // Apply click behavior
        d3.selectAll('.icon').on('click', click);
    }

    initializeIconBehavior();
   //d3.selectAll(".icon").on("click", function() {
    d3.selectAll(".icon").call(drag).on("click", click);

    function click() {
        if (isDragged) {
            // Reset flag and ignore click event if dragging occurred
            isDragged = false;
            console.log("Click Ignored by drag:");
            return;
        }
        const element = d3.select(this);
        if(!selectedElement) {
            alert("Source Icon  " + this.id);
            selectedElement = element;
        }
        else
        {
            alert("Target Icon " + this.id);
            if(selectedElement != element)
            {
                const x1 = parseInt(selectedElement.style("left")) + 50;
                const y1 = parseInt(selectedElement.style("top")) + 50;
                const x2 = parseInt(element.style("left")) + 50;
                const y2 = parseInt(element.style("top")) + 50;

                const line = svg.append("line")
                    .attr("x1", x1)
                    .attr("y1", y1)
                    .attr("x2", x2)
                    .attr("y2", y2)
                    .attr("class", "line");
                console.log("Drawing line:", x1, y1, x2, y2);
                lines.push({line, start: selectedElement, end: element});
                // Get the values from the input fields of the start and end elements
                const startValue = selectedElement.select("input").property("value");
                const endValue = element.select("input").property("value");

                // Push these values to the conns array
                conns.push({
                    start: startValue,
                    end: endValue
                });
            }
            selectedElement = null;
        }

    }

    function dragstarted(event, d) {
        isDragged = false; 
        d3.select(this).raise().classed("active", true);
    }

    function dragged(event, d) {
        isDragged = true; 
        d3.select(this).style("left", event.x + "px").style("top", event.y + "px");
        lines.forEach(function(l) {
            if (l.start.node() === this || l.end.node() === this) {
                l.line.attr("x1", parseInt(l.start.style("left")) + 50)
                      .attr("y1", parseInt(l.start.style("top")) + 50)
                      .attr("x2", parseInt(l.end.style("left")) + 50)
                      .attr("y2", parseInt(l.end.style("top")) + 50);
            }
        }, this);
    }

    function dragended(event, d) {
        d3.select(this).classed("active", false);
     }
    // Event listener for right-click on the body
    d3.select("body").on("contextmenu", function(event) {
        event.preventDefault(); // Prevent the default context menu
        menuPosition.x = event.pageX;
        menuPosition.y = event.pageY;
        showPopupMenu(event.pageX, event.pageY);
    });

    // Function to show the popup menu
    function showPopupMenu(x, y) {
        const popupMenu = d3.select("#popupMenu")
            .style("display", "block")
            .style("left", `${x}px`)
            .style("top", `${y}px`);

        popupMenu.selectAll(".menu-item").on("click", function() {
            //const iconId = d3.select(this).attr("data-icon");
            const iconType = d3.select(this).attr("data-icon");
            //const bgColor = d3.select(this).style("background-color");
            let bgColor;
            switch (iconType) {
                case 'Ess':
                    bgColor = '#f0f0f0'; // The specific color for Ess
                    break;
                case 'Pcs':
                    bgColor = '#add8e6'; // The specific color for Pcs
                    break;
                case 'Bms':
                    bgColor = '#90ee90'; // The specific color for Bms
                    break;
            }
            insertIconAtPosition(iconType, bgColor, menuPosition.x, menuPosition.y);
            //alert("Selected " + iconType);
            popupMenu.style("display", "none");
        });
    }
    // Function to insert the selected icon
    function insertIconAtPosition(iconType, color, x, y) {
        const icon = document.createElement("div");
        icon.className = "icon";
        icon.id = iconType;
        icon.textContent = iconType; // Or set to the appropriate content
        icon.style.left = `${x}px`;
        icon.style.top = `${y}px`;
        icon.style.backgroundColor = color;

        // Create an input field inside the icon
        const input = document.createElement("input");
        input.type = "text";
        input.value = iconType; // Set initial value to iconType
        input.className = "icon-name-input"; // Assign a class for styling
        input.addEventListener('click', function(event) {
            event.stopPropagation(); // Prevent the click event from bubbling up to the icon
        })
        input.onchange = function() {
            this.setAttribute('value', this.value); // Update value on change
        };
        icon.appendChild(input);
        document.body.appendChild(icon);
        initializeIconBehavior();
        //applyDragBehavior(d3.selectAll('.icon'));
    }

        // If you have additional logic to apply to new icons, add here
    
    // Hide popup menu when clicking elsewhere
    d3.select("body").on("click", function() {
        d3.select("#popupMenu").style("display", "none");
    });
    // Add event listener to the save button
    d3.select("#saveButton").on("click", function() {
        saveData();
    });

    function saveData() {
        // Gather data about the icons
        const icons = [];
        d3.selectAll('.icon').each(function() {
            const icon = d3.select(this);
            icons.push({
                id: icon.attr('id'),
                name: icon.select('input').property('value'),
                position: {
                    x: icon.style('left'),
                    y: icon.style('top')
                }
            });
        });

        // Create a map for quick access to names by ID
        const iconNamesById = new Map(icons.map(icon => [icon.id, icon.name]));

        // Gather data about the connections
        const connections = lines.map(l => ({
            startName: iconNamesById.get(l.start.attr('id')),
            endName: iconNamesById.get(l.end.attr('id'))
        }));
        // Gather data about the connections
        // const connections = lines.map(l => ({
        //     startId: l.start.attr('id'),
        //     endId: l.end.attr('id')
        // }));

        // Log the data to the console
        console.log('Icons:', icons);
        console.log('Connections:', connections);
        console.log('Conns:', conns);
    }

});


// document.addEventListener("DOMContentLoaded", function() {
//     let selectedElement = null;
//     const lines = [];
//     const svg = d3.select("body").append("svg")
//         .attr("width", window.innerWidth)
//         .attr("height", window.innerHeight)
//         .style("position", "absolute")
//         .style("top", 0)
//         .style("left", 0);

//     const drag = d3.drag()
//         .on("start", dragstarted)
//         .on("drag", dragged)
//         .on("end", dragended);

//     d3.selectAll(".icon").call(drag).on("click", click);

//     function click() {
//         const element = d3.select(this);
//         if (!selectedElement) {
//             selectedElement = element;
//         } else {
//             const x1 = parseInt(selectedElement.style("left")) + 50;
//             const y1 = parseInt(selectedElement.style("top")) + 50;
//             const x2 = parseInt(element.style("left")) + 50;
//             const y2 = parseInt(element.style("top")) + 50;

//             const line = svg.append("line")
//                 .attr("x1", x1)
//                 .attr("y1", y1)
//                 .attr("x2", x2)
//                 .attr("y2", y2)
//                 .attr("class", "line");

//             lines.push({line, start: selectedElement, end: element});
//             selectedElement = null;
//         }
//     }

//     function dragstarted(event, d) {
//         d3.select(this).raise().classed("active", true);
//     }

//     function dragged(event, d) {
//         d3.select(this).style("left", event.x + "px").style("top", event.y + "px");
//         lines.forEach(function(l) {
//             if (l.start.node() === this || l.end.node() === this) {
//                 l.line.attr("x1", parseInt(l.start.style("left")) + 50)
//                       .attr("y1", parseInt(l.start.style("top")) + 50)
//                       .attr("x2", parseInt(l.end.style("left")) + 50)
//                       .attr("y2", parseInt(l.end.style("top")) + 50);
//             }
//         }, this);
//     }

//     function dragended(event, d) {
//         d3.select(this).classed("active", false);
//     }
// });
