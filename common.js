function formatDate(timestamp) {
  const date = new Date(timestamp * 1000); // Convert seconds to milliseconds
  const options = {year: 'numeric',month: 'short',day: 'numeric',hour: '2-digit',minute: '2-digit'};
  return date.toLocaleDateString('en-US', options);
}
function loaduser(){
    fetch('http://121.36.103.216/api/user')
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
function fetchContent(id, contentDiv) {
    fetch(`http://121.36.103.216/con=${id}`)
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

let currentPage = 1;

function fetchBoardContent(page) {
  fetch(`http://121.36.103.216/p=${page}`)
    .then(response => {
      if (!response.ok) throw new Error('Network response was not ok');
      return response.json();
    })
    .then(data => {
      const boardItemsDiv = document.getElementById('boardItems');
      boardItemsDiv.innerHTML = '';
      data.forEach((item, index) => {
        const itemDiv = document.createElement('div');
        itemDiv.className = 'board-item';
        itemDiv.style.marginLeft = `${item.px * 20}px`;
        
        // Handle new line if marginLeft is 0
        if (item.px === 0) {
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
          if (itemId == '0') {
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
          document.getElementById('myModal').style.display = 'block';
        });
      });
    })
    .catch(error => {
      document.getElementById('boardItems').textContent = 'Error loading board content!';
    });
}

// Close the modal when the user clicks on <span> (x)
document.querySelector('.close').addEventListener('click', function() {
  document.getElementById('myModal').style.display = 'none';
});

// Send the message when the user clicks on the Send button
document.getElementById('sendMessage').addEventListener('click', function() {
  const title = document.getElementById('messageTitle').value;
  const content = document.getElementById('messageContent').value;
  const id = document.getElementById('messageId').value;

  fetch('http://121.36.103.216/api/sendmessage', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ id, title, content }),
  })
  .then(response => response.json())
  .then(data => {
    alert('Message sent successfully!');
    document.getElementById('myModal').style.display = 'none';
  })
  .catch(error => {
    alert('Error sending message!');
  });
});

// Close the modal if the user clicks anywhere outside of the modal
window.onclick = function(event) {
  if (event.target == document.getElementById('myModal')) {
    document.getElementById('myModal').style.display = 'none';
  }
};
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
loaduser();
fetchBoardContent(currentPage);