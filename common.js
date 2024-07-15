document.addEventListener('DOMContentLoaded', () => {
  let currentPage = 1;
  const modal = document.getElementById('myModal');
  const closeButton = document.querySelector('.close');
  const newMessageButton = document.getElementById('newMessage');
  const sendMessageButton = document.getElementById('sendMessage');

  function openModal() {
    modal.style.display = 'block';
  }

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
        data.forEach(item => {
          const itemDiv = document.createElement('div');
          itemDiv.className = 'board-item';
          itemDiv.style.marginLeft = `${item.px * 20}px`;
          
          if (item.px === '0') {
            itemDiv.style.clear = 'both';
          }
          
          const titleText = item.id === '0' ? `${item.title}` : `+ ${item.title}`;
          itemDiv.innerHTML = `<strong>${item.name} </strong>${formatDate(item.date)}
            <button class="modal-trigger" data-id="${item.id}">Reply</button><br>
            <strong class="item-title" data-id="${item.id}">${titleText}</strong>
            <div class="item-content" id="content-${item.id}"></div>`;
          boardItemsDiv.appendChild(itemDiv);
        });

        document.querySelectorAll('.item-title').forEach(titleElement => {
          titleElement.addEventListener('click', function() {
            const itemId = this.dataset.id;
            if (itemId === '0') {
              alert('No content available for this item.');
              return;
            }
            
            const contentDiv = this.nextElementSibling;
            if (contentDiv.style.display === 'none' || contentDiv.style.display === '') {
              fetchContent(itemId, contentDiv);
            } else {
              contentDiv.style.display = 'none';
            }
          });
        });

        document.querySelectorAll('.modal-trigger').forEach(button => {
          button.addEventListener('click', function() {
            const itemId = this.dataset.id;
            document.getElementById('messageId').value = itemId;
            document.getElementById('messageTitle').value = '';
            document.getElementById('messageContent').value = '';
            openModal();
          });
        });
      })
      .catch(error => {
        document.getElementById('boardItems').textContent = 'Error loading board content!';
      });
  }

  // Close the modal when the user clicks on <span> (x)
  closeButton.addEventListener('click', closeModal);

  // Close the modal if the user clicks anywhere outside of the modal
  window.addEventListener('click', function(event) {
    if (event.target === modal) {
      closeModal();
    }
  });

  // Open modal for "Post New Message"
  newMessageButton.addEventListener('click', function() {
    document.getElementById('messageId').value = 1;
    document.getElementById('messageTitle').value = '';
    document.getElementById('messageContent').value = '';
    openModal();
  });

  // Send the message when the user clicks on the Send button
  sendMessageButton.addEventListener('click', function() {
    const title = document.getElementById('messageTitle').value;
    const title_ = document.getElementById('messageTitle').value.trim();
    if(!title_) {
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
      if (!response.ok) {
        throw new Error('Network response was not ok');  
      }
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
    currentPage += 1;
    fetchBoardContent(currentPage);
  });
  document.getElementById('olderButton').addEventListener('click', function() {
    if (currentPage > 1) {
      currentPage -= 1;
      fetchBoardContent(currentPage);
    }
  });
  function formatDate(timestamp) {
    const date = new Date(timestamp * 1000);
    const options = {year: 'numeric',month: 'short',day: 'numeric',hour: '2-digit',minute: '2-digit'};
    return date.toLocaleDateString('en-US', options);
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
  fetchBoardContent(currentPage);
});
