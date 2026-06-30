let lastScrollTop = 0;
fetch('/navbar.html')
.then(response => response.text())
.then(data => {
    document.getElementById("navbar-container").innerHTML = data;
    setTimeout(function() {
        document.getElementById('drop').href="/login?"+window.location.href;
        loaduser();
        window.addEventListener('scroll', function() {
            const navbar = document.getElementById('navbar');
            let scrollTop = window.scrollY || document.documentElement.scrollTop;
            if (scrollTop > lastScrollTop&&lastScrollTop!==0)navbar.style.top = '-60px';
            else navbar.style.top = '0';
            lastScrollTop = scrollTop;
        });
    }, 0);
})
.catch(error => console.log("load_error",location,id,error));
function loaduser(){
    fetch('/api/user', {
        method: 'GET',
    })
    .then(response => {
        if (!response.ok) throw new Error('Network response was not ok');
        return response.json();
    })
    .then(data => {
        if(!data.name);
        else {
            document.getElementById('drop').style.display = 'none';
            document.getElementById('username').style.display = 'inline';
            document.getElementById('username').textContent =data.name;
        }
    })
    .catch(error => {
        console.log(error);
    });
}
