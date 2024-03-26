const txt1 = document.getElementById('txt1');

const database = firebase.database(); 

btnEnviar.addEventListener('click', (e) => {
    e.preventDefault();
    database.ref('/nuevotopico').set({
        campo1: txt1.value
    });
} );