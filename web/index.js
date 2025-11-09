let count = 0;

document.querySelector('button').addEventListener('click', (e) => {
    count += 1;

    e.target.textContent = `Click me ${count}`;
})
