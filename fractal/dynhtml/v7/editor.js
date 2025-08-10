function renderEditor() {
    const editorDiv = document.getElementById('editor');
    editorDiv.innerHTML = "";
    console.log("running new editor");
    return;
  
    // Structure Section
    const structHeader = document.createElement('h3');
    structHeader.innerText = "Structure:";
    editorDiv.appendChild(structHeader);
  
    const addStructBtn = createButton('Add Structure', addNewStructure);
    editorDiv.appendChild(addStructBtn);
  
    frameData.frames.structure.forEach((item, idx) => {
      const div = document.createElement('div');
      div.style.margin = "5px";
      div.style.paddingLeft = "10px";
      div.innerHTML = `<b>${item.name}</b>`;
  
      const editBtn = createButton('Edit', () => editStructureItem(idx));
      const addBeforeBtn = createButton('Add Before', () => addStructureBefore(idx));
      const addAfterBtn = createButton('Add After', () => addStructureAfter(idx));
      const deleteBtn = createButton('Delete', () => deleteStructureItem(idx));
      const moveUpBtn = createButton('Move Up', () => moveStructureUp(idx));
      const moveDownBtn = createButton('Move Down', () => moveStructureDown(idx));
      const testBtn = createButton('Test', () => editStructureItem(idx));
  
      div.appendChild(editBtn);
      div.appendChild(addBeforeBtn);
      div.appendChild(addAfterBtn);
      div.appendChild(deleteBtn);
      div.appendChild(moveUpBtn);
      div.appendChild(moveDownBtn);
  
      editorDiv.appendChild(div);
    });
  
    // Template Section
    const tmplHeader = document.createElement('h3');
    tmplHeader.innerText = "Templates:";
    editorDiv.appendChild(tmplHeader);
  
    const addTemplateBtn = createButton('Add New Template', addNewTemplate);
    editorDiv.appendChild(addTemplateBtn);
  
    for (const key in frameData.frames.templates) {
      const div = document.createElement('div');
      div.style.margin = "5px";
      div.style.paddingLeft = "10px";
      div.innerHTML = `<b>${key}</b>`;
  
      const editBtn = createButton('Edit', () => editTemplateItem(key));
      const deleteBtn = createButton('Delete', () => deleteTemplateItem(key));
  
      div.appendChild(editBtn);
      div.appendChild(deleteBtn);
  
      editorDiv.appendChild(div);
    }
  }
  
  function createButton(label, onClick) {
    const btn = document.createElement('button');
    btn.innerText = label;
    btn.onclick = onClick;
    return btn;
  }
  