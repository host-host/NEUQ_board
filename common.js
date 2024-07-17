/********************************************
 * 版权声明
 * 
 * 本程序由 刘峻畅 编写。
 * 版权所有：Copyright (c) 2024 刘峻畅。
 * 
 * 本程序是开源的，你可以自由地复制、分发和修改它，
 * 但请保留此版权声明。
 * 
 * 注意：使用本程序时，请遵守相关法律法规。 
 ********************************************/
document.addEventListener('DOMContentLoaded', () => {
  let currentid = [1];
  const modal = document.getElementById('myModal');
  const closeButton = document.querySelector('.close');
  const newMessageButton = document.getElementById('newMessage');
  const sendMessageButton = document.getElementById('sendMessage');
  function closeModal() {
    modal.style.display = 'none';
  }
  function fetchBoardContent(page) {
    fetch(`/p=${page}`)
      .then(response => {
        if (!response.ok) throw new Error('Network response was not ok');
        return response.json();
      })
      .then(data => {
        const boardItemsDiv = document.getElementById('boardItems');
        boardItemsDiv.innerHTML = '';
        let newid=-1;
        data.forEach(item => {
          const itemDiv = document.createElement('div');
          itemDiv.className = 'board-item';
          itemDiv.style.marginLeft = `${item.px * 20}px`;
          if (item.px === '0')itemDiv.style.clear = 'both';
          const titleText = item.id <0 ? `${item.title}` : `+ ${item.title}`;
          newid=item.id;
          itemDiv.innerHTML = `<strong>${item.name} </strong>${formatDate(item.date)}<button class="modal-trigger" data-id="${item.id}">Reply</button><br><strong class="item-title" data-id="${item.id}">${titleText}</strong><div class="item-content" id="content-${item.id}"></div>`;
          boardItemsDiv.appendChild(itemDiv);
        });
        if(newid!=-1)currentid.push(newid);
        document.querySelectorAll('.item-title').forEach(titleElement => {
          titleElement.addEventListener('click', function() {
            const itemId = this.dataset.id;
            if (itemId <0) {
              alert('No content available for this item.');
              return;
            }
            const contentDiv = this.nextElementSibling;
            if (contentDiv.style.display === 'none' || contentDiv.style.display === '')fetchContent(itemId, contentDiv);
            else contentDiv.style.display = 'none';
          });
        });
        document.querySelectorAll('.modal-trigger').forEach(button => {
          button.addEventListener('click', function() {
            const itemId = this.dataset.id;
            document.getElementById('messageId').value = itemId;
            document.getElementById('messageTitle').value = '';
            document.getElementById('messageContent').value = '';
            modal.style.display='block';
          });
        });
      })
      .catch(error => {document.getElementById('boardItems').textContent = 'Error loading board content!';});
  }
  closeButton.addEventListener('click', closeModal);
  newMessageButton.addEventListener('click', function() {
    document.getElementById('messageId').value = 1;
    document.getElementById('messageTitle').value = '';
    document.getElementById('messageContent').value = '';
    modal.style.display='block';
  });
  sendMessageButton.addEventListener('click', function() {
    const title = document.getElementById('messageTitle').value;
    if(!title.trim()) {
      alert('Title cannot be empty.');
      return;
    }
    const content = document.getElementById('messageContent').value;
    const id = document.getElementById('messageId').value;
    const dataString = JSON.stringify(title) + '\0' + content + '\0' + id + '\0';
    fetch('/api/sendmessage', {
      method: 'POST',
      headers: {'Content-Type': 'text/plain','Content-Length': dataString.length},
      body: dataString
    })
    .then(response => {
      if (!response.ok)throw new Error('Network response was not ok');  
      return response.text();
    })
    .then(data => {
      if(data=='ok')location.reload();
      else alert(data);
    })
    .catch(error => {  
      alert('Error sending message!');
    });
  });
  document.getElementById('newerButton').addEventListener('click', function() {
    if(currentid.length>1)currentid.pop();
    if(currentid.length>1)currentid.pop();
    fetchBoardContent(currentid[currentid.length-1]);
  });
  document.getElementById('olderButton').addEventListener('click', function() {
    fetchBoardContent(currentid[currentid.length-1]);
  });
  function formatDate(timestamp) {
    const date=new Date(timestamp*1000);
    const year=date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hour = String(date.getHours()).padStart(2, '0');
    const minute = String(date.getMinutes()).padStart(2, '0');
    const second = String(date.getSeconds()).padStart(2, '0');
    return `${year}-${month}-${day} ${hour}:${minute}:${second}`;
}
  function fetchContent(id, contentDiv) {
    fetch(`/con=${id}`)
    .then(response => {
      if (!response.ok) throw new Error('Network response was not ok');
      return response.text();
    })
    .then(data => {
      contentDiv.innerHTML = data;
      contentDiv.style.display = 'block';
    })
    .catch(error => {
      contentDiv.innerHTML = 'Error loading content!';
      contentDiv.style.display = 'block';
    });
}
function loaduser(){
  fetch('/api/user')
  .then(response => {
      if (!response.ok) throw new Error('Network response was not ok');
      return response.text();
  })
  .then(data => {
      if (data === "Not_Logged_In") {
      document.getElementById('userStatus').textContent = data;
      } else {
      document.getElementById('userStatus').textContent = 'Your id is: ' + data;
      document.getElementById('profile').style.display = 'inline';
      document.getElementById('signOutLink').style.display = 'inline';
      document.getElementById('signInLink').style.display = 'none';
      }
  })
  .catch(error => {
      document.getElementById('userStatus').textContent = 'Error loading user status!';
  });
}
  loaduser();
  fetchBoardContent(currentid[currentid.length-1]);
});
