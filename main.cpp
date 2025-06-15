#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <stack>
#include <deque>

using namespace std;

// Estructura para representar un i-nodo
struct INode {
    int id; // Identificador único
    string name; // Nombre del archivo/directorio
    bool is_directory; // true si es directorio, false si es archivo
    int size; // Tamaño en bytes
    time_t creation_time; // Fecha de creación
    time_t modification_time; // Fecha de última modificación
    string permissions; // Permisos (ej. "rwxr-xr--")
    vector<int> blocks; // Bloques de datos (simulados)
    vector<int> children; // IDs de hijos (para directorios)
    int parent; // ID del padre

    INode() : id(-1), is_directory(false), size(0), parent(-1) {
        time(&creation_time);
        modification_time = creation_time;
        permissions = "rwxr-xr-x";
    }
};

// Clase principal del sistema de archivos
class SimpleFileSystem {
private:
    map<int, INode> inodes; // Todos los i-nodos del sistema
    int current_inode; // INode actual (directorio de trabajo)
    int next_inode_id; // Siguiente ID disponible
    deque<string> command_history; // Historial de comandos
    string fs_file = "filesystem.dat"; // Archivo de persistencia

    // Métodos privados
    void saveToDisk();
    void loadFromDisk();
    void updateModificationTime(int inode_id);
    string getAbsolutePath(int inode_id);
    void recursiveList(int inode_id, int depth = 0);

public:
    SimpleFileSystem();
    ~SimpleFileSystem();
    
    // Métodos públicos (interfaz del sistema de archivos)
    void mkdir(const string& name);
    void touch(const string& name);
    void ls(bool recursive = false, bool show_inodes = false);
    void cd(const string& path);
    void rm(const string& name);
    void mv(const string& old_name, const string& new_name);
    void chmod(const string& name, const string& mode);
    void find(const string& name);
    void history();
    void pwd();
    
    // Métodos para la shell
    void runShell();
    void executeCommand(const string& command);
};

// Constructor - inicializa el sistema de archivos
SimpleFileSystem::SimpleFileSystem() : next_inode_id(1) {
    // Crear el i-nodo raíz si no existe
    if (inodes.empty()) {
        INode root;
        root.id = 0;
        root.name = "/";
        root.is_directory = true;
        root.parent = 0; // El root es su propio padre
        inodes[0] = root;
        next_inode_id = 1;
    }
    
    current_inode = 0; // Empezamos en el directorio raíz
    loadFromDisk(); // Cargar datos persistentes
}

// Destructor - guarda el estado al salir
SimpleFileSystem::~SimpleFileSystem() {
    saveToDisk();
}

// Guardar el sistema de archivos a disco
void SimpleFileSystem::saveToDisk() {
    ofstream out(fs_file, ios::binary);
    if (!out) {
        cerr << "Error al guardar el sistema de archivos" << endl;
        return;
    }
    
    // Guardar el número total de i-nodos
    size_t count = inodes.size();
    out.write(reinterpret_cast<char*>(&count), sizeof(count));
    
    // Guardar cada i-nodo
    for (const auto& pair : inodes) {
        const INode& node = pair.second;
        out.write(reinterpret_cast<const char*>(&node.id), sizeof(node.id));
        
        // Guardar campos de cadena
        size_t len = node.name.size();
        out.write(reinterpret_cast<char*>(&len), sizeof(len));
        out.write(node.name.c_str(), len);
        
        out.write(reinterpret_cast<const char*>(&node.is_directory), sizeof(node.is_directory));
        out.write(reinterpret_cast<const char*>(&node.size), sizeof(node.size));
        out.write(reinterpret_cast<const char*>(&node.creation_time), sizeof(node.creation_time));
        out.write(reinterpret_cast<const char*>(&node.modification_time), sizeof(node.modification_time));
        
        len = node.permissions.size();
        out.write(reinterpret_cast<char*>(&len), sizeof(len));
        out.write(node.permissions.c_str(), len);
        
        // Guardar hijos
        len = node.children.size();
        out.write(reinterpret_cast<char*>(&len), sizeof(len));
        for (int child : node.children) {
            out.write(reinterpret_cast<char*>(&child), sizeof(child));
        }
        
        out.write(reinterpret_cast<const char*>(&node.parent), sizeof(node.parent));
    }
    
    out.close();
}

// Cargar el sistema de archivos desde disco
void SimpleFileSystem::loadFromDisk() {
    ifstream in(fs_file, ios::binary);
    if (!in) {
        // Archivo no existe, empezar con sistema vacío
        return;
    }
    
    inodes.clear();
    
    // Leer el número total de i-nodos
    size_t count;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (size_t i = 0; i < count; ++i) {
        INode node;
        
        // Leer campos básicos
        in.read(reinterpret_cast<char*>(&node.id), sizeof(node.id));
        
        size_t len;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        node.name.resize(len);
        in.read(&node.name[0], len);
        
        in.read(reinterpret_cast<char*>(&node.is_directory), sizeof(node.is_directory));
        in.read(reinterpret_cast<char*>(&node.size), sizeof(node.size));
        in.read(reinterpret_cast<char*>(&node.creation_time), sizeof(node.creation_time));
        in.read(reinterpret_cast<char*>(&node.modification_time), sizeof(node.modification_time));
        
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        node.permissions.resize(len);
        in.read(&node.permissions[0], len);
        
        // Leer hijos
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        node.children.resize(len);
        for (size_t j = 0; j < len; ++j) {
            in.read(reinterpret_cast<char*>(&node.children[j]), sizeof(node.children[j]));
        }
        
        in.read(reinterpret_cast<char*>(&node.parent), sizeof(node.parent));
        
        inodes[node.id] = node;
        
        // Actualizar el siguiente ID disponible
        if (node.id >= next_inode_id) {
            next_inode_id = node.id + 1;
        }
    }
    
    in.close();
}

// Actualizar tiempo de modificación
void SimpleFileSystem::updateModificationTime(int inode_id) {
    time(&inodes[inode_id].modification_time);
}

// Crear un directorio
void SimpleFileSystem::mkdir(const string& name) {
    // Verificar si ya existe un hijo con ese nombre
    for (int child_id : inodes[current_inode].children) {
        if (inodes[child_id].name == name) {
            cerr << "mkdir: no se puede crear el directorio '" << name 
                 << "': El archivo ya existe" << endl;
            return;
        }
    }
    
    // Crear nuevo i-nodo
    INode new_dir;
    new_dir.id = next_inode_id++;
    new_dir.name = name;
    new_dir.is_directory = true;
    new_dir.parent = current_inode;
    
    // Añadir al directorio actual
    inodes[current_inode].children.push_back(new_dir.id);
    inodes[new_dir.id] = new_dir;
    
    updateModificationTime(current_inode);
    cout << "Directorio '" << name << "' creado." << endl;
}

// Crear un archivo
void SimpleFileSystem::touch(const string& name) {
    // Verificar si ya existe
    for (int child_id : inodes[current_inode].children) {
        if (inodes[child_id].name == name) {
            // Actualizar tiempo de modificación
            updateModificationTime(child_id);
            cout << "Archivo '" << name << "' actualizado." << endl;
            return;
        }
    }
    
    // Crear nuevo i-nodo
    INode new_file;
    new_file.id = next_inode_id++;
    new_file.name = name;
    new_file.is_directory = false;
    new_file.parent = current_inode;
    
    // Añadir al directorio actual
    inodes[current_inode].children.push_back(new_file.id);
    inodes[new_file.id] = new_file;
    
    updateModificationTime(current_inode);
    cout << "Archivo '" << name << "' creado." << endl;
}

// Listar contenido
void SimpleFileSystem::ls(bool recursive, bool show_inodes) {
    if (!recursive) {
        for (int child_id : inodes[current_inode].children) {
            const INode& child = inodes[child_id];
            if (show_inodes) {
                cout << child.id << "\t";
            }
            cout << child.name;
            if (child.is_directory) {
                cout << "/";
            }
            cout << "\t" << child.permissions << "\t" << child.size << " bytes" << endl;
        }
    } else {
        recursiveList(current_inode);
    }
}

// Listado recursivo
void SimpleFileSystem::recursiveList(int inode_id, int depth) {
    const INode& node = inodes[inode_id];
    
    // Imprimir indentación
    for (int i = 0; i < depth; ++i) {
        cout << "  ";
    }
    
    cout << node.name;
    if (node.is_directory) {
        cout << "/";
    }
    cout << "\t" << node.permissions << "\t" << node.size << " bytes" << endl;
    
    // Listar hijos recursivamente
    if (node.is_directory) {
        for (int child_id : node.children) {
            recursiveList(child_id, depth + 1);
        }
    }
}

// Cambiar de directorio
void SimpleFileSystem::cd(const string& path) {
    if (path.empty()) {
        return;
    }
    
    if (path == "/") {
        current_inode = 0; // Raíz
        return;
    }
    
    if (path == "..") {
        if (current_inode != 0) { // No podemos subir de la raíz
            current_inode = inodes[current_inode].parent;
        }
        return;
    }
    
    // Buscar el directorio en los hijos
    for (int child_id : inodes[current_inode].children) {
        if (inodes[child_id].name == path && inodes[child_id].is_directory) {
            current_inode = child_id;
            return;
        }
    }
    
    cerr << "cd: no existe el directorio '" << path << "'" << endl;
}

// Eliminar archivo/directorio
void SimpleFileSystem::rm(const string& name) {
    auto& children = inodes[current_inode].children;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (inodes[*it].name == name) {
            if (inodes[*it].is_directory && !inodes[*it].children.empty()) {
                cerr << "rm: no se puede eliminar '" << name 
                     << "': El directorio no está vacío" << endl;
                return;
            }
            
            // Eliminar de la lista de hijos
            children.erase(it);
            
            // Eliminar el i-nodo (y todos sus hijos si es directorio)
            stack<int> to_delete;
            to_delete.push(*it);
            
            while (!to_delete.empty()) {
                int id = to_delete.top();
                to_delete.pop();
                
                // Añadir hijos a la pila si es directorio
                if (inodes[id].is_directory) {
                    for (int child_id : inodes[id].children) {
                        to_delete.push(child_id);
                    }
                }
                
                // Eliminar el i-nodo
                inodes.erase(id);
            }
            
            updateModificationTime(current_inode);
            cout << "Eliminado '" << name << "'" << endl;
            return;
        }
    }
    
    cerr << "rm: no existe '" << name << "'" << endl;
}

// Renombrar/mover archivo
void SimpleFileSystem::mv(const string& old_name, const string& new_name) {
    // Buscar el archivo/directorio
    for (int child_id : inodes[current_inode].children) {
        if (inodes[child_id].name == old_name) {
            // Verificar que el nuevo nombre no existe
            for (int other_id : inodes[current_inode].children) {
                if (inodes[other_id].name == new_name) {
                    cerr << "mv: no se puede renombrar '" << old_name 
                         << "' a '" << new_name << "': El archivo ya existe" << endl;
                    return;
                }
            }
            
            // Cambiar el nombre
            inodes[child_id].name = new_name;
            updateModificationTime(child_id);
            updateModificationTime(current_inode);
            cout << "Renombrado '" << old_name << "' a '" << new_name << "'" << endl;
            return;
        }
    }
    
    cerr << "mv: no existe '" << old_name << "'" << endl;
}

// Cambiar permisos
void SimpleFileSystem::chmod(const string& name, const string& mode) {
    for (int child_id : inodes[current_inode].children) {
        if (inodes[child_id].name == name) {
            // Validar formato de permisos (simplificado)
            if (mode.size() == 9 || mode.size() == 3) {
                inodes[child_id].permissions = mode;
                updateModificationTime(child_id);
                cout << "Permisos de '" << name << "' cambiados a '" << mode << "'" << endl;
            } else {
                cerr << "chmod: formato de permisos inválido" << endl;
            }
            return;
        }
    }
    
    cerr << "chmod: no existe '" << name << "'" << endl;
}

// Buscar archivo/directorio
void SimpleFileSystem::find(const string& name) {
    stack<int> to_search;
    to_search.push(current_inode);
    bool found = false;
    
    while (!to_search.empty()) {
        int id = to_search.top();
        to_search.pop();
        
        const INode& node = inodes[id];
        if (node.name.find(name) != string::npos) {
            cout << getAbsolutePath(id) << endl;
            found = true;
        }
        
        if (node.is_directory) {
            for (int child_id : node.children) {
                to_search.push(child_id);
            }
        }
    }
    
    if (!found) {
        cout << "No se encontraron coincidencias para '" << name << "'" << endl;
    }
}

// Mostrar historial de comandos
void SimpleFileSystem::history() {
    int i = 1;
    for (const string& cmd : command_history) {
        cout << " " << i++ << "  " << cmd << endl;
    }
}

// Obtener ruta absoluta
string SimpleFileSystem::getAbsolutePath(int inode_id) {
    if (inode_id == 0) {
        return "/";
    }
    
    vector<string> path_parts;
    int current = inode_id;
    
    while (current != 0) {
        path_parts.push_back(inodes[current].name);
        current = inodes[current].parent;
    }
    
    reverse(path_parts.begin(), path_parts.end());
    
    string path = "/";
    for (size_t i = 0; i < path_parts.size(); ++i) {
        if (i != 0) {
            path += "/";
        }
        path += path_parts[i];
    }
    
    return path;
}

// Mostrar directorio actual
void SimpleFileSystem::pwd() {
    cout << getAbsolutePath(current_inode) << endl;
}

// Ejecutar un comando
void SimpleFileSystem::executeCommand(const string& command) {
    command_history.push_back(command);
    if (command_history.size() > 100) {
        command_history.pop_front();
    }
    
    istringstream iss(command);
    string cmd;
    iss >> cmd;
    
    if (cmd == "mkdir") {
        string dirname;
        if (iss >> dirname) {
            mkdir(dirname);
        } else {
            cerr << "Uso: mkdir <nombre_directorio>" << endl;
        }
    } else if (cmd == "touch") {
        string filename;
        if (iss >> filename) {
            touch(filename);
        } else {
            cerr << "Uso: touch <nombre_archivo>" << endl;
        }
    } else if (cmd == "ls") {
        string option;
        bool recursive = false;
        bool show_inodes = false;
        
        while (iss >> option) {
            if (option == "-R") {
                recursive = true;
            } else if (option == "-i") {
                show_inodes = true;
            }
        }
        
        ls(recursive, show_inodes);
    } else if (cmd == "cd") {
        string path;
        if (iss >> path) {
            cd(path);
        } else {
            cd("/"); // Sin argumento, ir a home (en este caso raíz)
        }
    } else if (cmd == "rm") {
        string name;
        if (iss >> name) {
            rm(name);
        } else {
            cerr << "Uso: rm <nombre>" << endl;
        }
    } else if (cmd == "mv") {
        string old_name, new_name;
        if (iss >> old_name >> new_name) {
            mv(old_name, new_name);
        } else {
            cerr << "Uso: mv <nombre_viejo> <nombre_nuevo>" << endl;
        }
    } else if (cmd == "chmod") {
        string name, mode;
        if (iss >> name >> mode) {
            chmod(name, mode);
        } else {
            cerr << "Uso: chmod <nombre> <permisos>" << endl;
        }
    } else if (cmd == "find") {
        string name;
        if (iss >> name) {
            find(name);
        } else {
            cerr << "Uso: find <nombre>" << endl;
        }
    } else if (cmd == "history") {
        history();
    } else if (cmd == "pwd") {
        pwd();
    } else if (cmd == "exit" || cmd == "quit") {
        saveToDisk();
        exit(0);
    } else if (!cmd.empty()) {
        cerr << "Comando no reconocido: " << cmd << endl;
    }
}

// Shell interactiva
void SimpleFileSystem::runShell() {
    string input;
    
    cout << "Sistema de Archivos Simple - Ingrese 'help' para ver comandos disponibles" << endl;
    
    while (true) {
        cout << getAbsolutePath(current_inode) << " $ ";
        getline(cin, input);
        
        if (input == "help") {
            cout << "Comandos disponibles:\n"
                 << "  mkdir <nombre>      - Crear directorio\n"
                 << "  touch <nombre>      - Crear archivo\n"
                 << "  ls [-R] [-i]        - Listar contenido (-R recursivo, -i mostrar i-nodos)\n"
                 << "  cd [directorio]     - Cambiar directorio\n"
                 << "  rm <nombre>         - Eliminar archivo/directorio\n"
                 << "  mv <viejo> <nuevo>  - Renombrar/mover\n"
                 << "  chmod <nom> <perm>  - Cambiar permisos\n"
                 << "  find <nombre>       - Buscar archivos\n"
                 << "  history             - Mostrar historial\n"
                 << "  pwd                 - Mostrar directorio actual\n"
                 << "  exit/quit           - Salir\n";
        } else {
            executeCommand(input);
        }
    }
}

// Función principal
int main() {
    SimpleFileSystem fs;
    fs.runShell();
    return 0;
}