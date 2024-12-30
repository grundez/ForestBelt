#include <GL/glew.h>
#include <GL/glut.h>
#include <iostream>
#include <cmath>
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>

// Переменные для загрузки моделей
const aiScene* tractorScene;
const aiScene* grassScene;
const aiScene* treeScene;

GLuint tractorDisplayList;
GLuint grassDisplayList;
GLuint treesDisplayList;

// Текстуры
GLuint grassTexture;
GLuint tractorTexture;
GLuint barkTexture;
GLuint leafTexture;
GLuint skyTexture;

// Переменные для позиции камеры и углов
float camX = 100.0f, camY = 100.0f, camZ = 100.0f;
float camSpeed = 0.1f; // Скорость перемещения камеры
float pitch = 0.0f, yaw = -90.0f; // Углы поворота камеры
float lastX = 400, lastY = 300; // Координаты последней позиции мыши
bool firstMouse = true; // Флаг для первого движения мыши

GLuint loadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

void initTextures() {
    grassTexture = loadTexture("./src/grassTexUV.jpg");
    tractorTexture = loadTexture("./src/TextureTractor.jpg");
    barkTexture = loadTexture("./src/bark_0004.jpg");
    leafTexture = loadTexture("./src/DB2X2_L01.png");
    skyTexture = loadTexture("./src/skyboxUV.png");

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

GLuint createDisplayListFromScene(const aiScene* scene, GLuint textureID) {
    GLuint listID = glGenLists(1);
    glNewList(listID, GL_COMPILE);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];

        glBegin(GL_TRIANGLES);
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            const aiFace& face = mesh->mFaces[j];

            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];

                if (mesh->HasNormals()) {
                    glNormal3fv(&mesh->mNormals[index].x);
                }

                if (mesh->HasTextureCoords(0)) {
                    glTexCoord2f(mesh->mTextureCoords[0][index].x, mesh->mTextureCoords[0][index].y);
                }

                glVertex3fv(&mesh->mVertices[index].x);
            }
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);

    glEndList();
    return listID;
}

GLuint createTreeDisplayList(const aiScene* scene, GLuint barkTextureID, GLuint leafTextureID) {
    GLuint listID = glGenLists(1);
    glNewList(listID, GL_COMPILE);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // Определяем, это ствол или листва
        aiString materialName;
        material->Get(AI_MATKEY_NAME, materialName);
        std::cout << "Material name: " << materialName.C_Str() << std::endl;
        GLuint textureID = (std::string(materialName.C_Str()).find("DB2X2_L01") != std::string::npos) ? leafTextureID : barkTextureID;

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glBegin(GL_TRIANGLES);
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            const aiFace& face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];
                if (mesh->HasNormals()) {
                    glNormal3fv(&mesh->mNormals[index].x);
                }
                if (mesh->HasTextureCoords(0)) {
                    glTexCoord2f(mesh->mTextureCoords[0][index].x, mesh->mTextureCoords[0][index].y);
                }
                glVertex3fv(&mesh->mVertices[index].x);
            }
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glEndList();
    return listID;
}

void loadTreeModel(const char* filePath, GLuint& displayList, GLuint barkTextureID, GLuint leafTextureID) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!scene) {
        std::cerr << "Error loading tree model: " << importer.GetErrorString() << std::endl;
        return;
    }

    displayList = createTreeDisplayList(scene, barkTextureID, leafTextureID);
}

void loadModel(const char* filePath, const char* texturePath, GLuint& displayList, GLuint& textureID) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene) {
        std::cerr << "Error loading model: " << importer.GetErrorString() << std::endl;
        return;
    }

    textureID = loadTexture(texturePath);
    displayList = createDisplayListFromScene(scene, textureID);
}

void initOpenGL() {
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f); // Цвет фона (небо)
    glEnable(GL_DEPTH_TEST);             // Включаем тест глубины

    // Включить смешивание для прозрачности
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Включить альфа-тестирование (удаляет полностью прозрачные пиксели)
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f); // Удалить пиксели с альфа-каналом < 0.1

    // Включение освещения
    float lightAmbient[] = { 0.8f, 0.8f, 0.8f, 1.0f }; // Светлый фоновый свет
    float lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // Диффузное освещение
    float lightPosition[] = { 1.0f, 1.0f, 1.0f, 0.0f }; // Позиция света

    glEnable(GL_LIGHTING);  // Включить освещение
    glEnable(GL_LIGHT0);    // Включить источник света 0

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);   // Установить фоновой свет
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);   // Установить диффузное освещение
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition); // Установить позицию света

    // Инициализация текстур
    initTextures();

    // Загрузка моделей
    loadModel("./src/Tractor.obj", "./src/TextureTractor.jpg", tractorDisplayList, tractorTexture);
    loadModel("./src/grassObj.obj", "./src/grassTexUV.jpg", grassDisplayList, grassTexture);
    loadTreeModel("./src/Tree.obj", treesDisplayList, barkTexture, leafTexture);

    glDisable(GL_LIGHTING);
}

void drawGround() {
    if (grassDisplayList != 0) {
        glPushMatrix();
        glScalef(0.6f, 1.0f, 1.5f); // Уменьшаем землю
        glTranslatef(0.0f, -0.5f, 100.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glCallList(grassDisplayList);
        glPopMatrix();
    }
}

void drawTree() {
    if (treesDisplayList != 0) {
        float x = 5.0f;
        float z = -5.0f;
        for (int i = 0; i < 27; i++) {
            if (i < 7) {
                // 1 ряд
                glPushMatrix();
                glScalef(15.0f, 15.0f, 15.0f); // Размер дерева
                i % 2 == 0 ? glTranslatef(x, 0.5f, z) : glTranslatef(x, 0.5f, z + 0.5f); // Позиция дерева
                glCallList(treesDisplayList);
                glPopMatrix();
                x -= 1.5f;

                // 2 ряд
                glPushMatrix();
                glScalef(15.0f, 15.0f, 15.0f); // Размер дерева
                i % 2 == 0 ? glTranslatef(x, 0.5f, z + 1.0f) : glTranslatef(x, 0.5f, z + 1.5f); // Позиция дерева
                glCallList(treesDisplayList);
                glPopMatrix();
            }
            else {
                x = 5.0f;
                glPushMatrix();
                glScalef(15.0f, 15.0f, 15.0f); // Размер дерева
                i % 2 == 0 ? glTranslatef(x, 0.5f, z) : glTranslatef(x + 0.5f, 0.5f, z); // Позиция дерева
                glCallList(treesDisplayList);
                glPopMatrix();
                z += 1.5f;
            }
        }
    }
}


void drawModel() {
    if (tractorDisplayList != 0) {
        glPushMatrix();
        glScalef(0.5f, 0.5f, 0.5f); // Уменьшаем трактор
        glRotatef(-15.0f, 0.0f, 1.0f, 0.0f); // Поворачиваем трактор
        glTranslatef(-60.0f, 55.0f, -100.0f); // Поднимаем трактор
        glCallList(tractorDisplayList);
        glPopMatrix();

        glPushMatrix();
        glScalef(0.5f, 0.5f, 0.5f); // Уменьшаем трактор
        glRotatef(5.0f, 0.0f, 1.0f, 0.0f); // Поворачиваем трактор
        glTranslatef(20.0f, 55.0f, 50.0f); // Поднимаем трактор
        glCallList(tractorDisplayList);
        glPopMatrix();
    }
}

void drawSkybox(float size) {
    glPushMatrix();
    glDisable(GL_LIGHTING);  // Отключаем освещение для skybox

    // Включаем текстурирование
    glEnable(GL_TEXTURE_2D);

    // Устанавливаем голубой фон, если текстура не используется
    glColor3f(0.53f, 0.81f, 0.92f);

    // Передняя грань
    glBindTexture(GL_TEXTURE_2D, grassTexture); // Используйте текстуру неба
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-size, -size, -size);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(size, -size, -size);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(size, size, -size);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-size, size, -size);
    glEnd();

    // Остальные грани куба добавьте аналогично, заменяя координаты и ориентацию.

    glEnable(GL_LIGHTING); // Включаем обратно освещение
    glPopMatrix();
}

// Функция загрузки и отрисовки куба
void drawTexturedCube(float size, GLuint textureID) {
    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    float halfSize = size / 2.0f;

    glBegin(GL_QUADS);

    // Верхняя грань (Top)
    glTexCoord2f(0.25f, 0.0f); glVertex3f(-halfSize, halfSize, -halfSize); // Верхний левый
    glTexCoord2f(0.50f, 0.0f); glVertex3f(halfSize, halfSize, -halfSize);  // Верхний правый
    glTexCoord2f(0.50f, 0.333f); glVertex3f(halfSize, halfSize, halfSize);   // Нижний правый
    glTexCoord2f(0.25f, 0.333f); glVertex3f(-halfSize, halfSize, halfSize);  // Нижний левый

    // Нижняя грань (Bottom)
    glTexCoord2f(0.25f, 1.0f); glVertex3f(-halfSize, -halfSize, halfSize);  // Верхний левый
    glTexCoord2f(0.50f, 1.0f); glVertex3f(halfSize, -halfSize, halfSize);   // Верхний правый
    glTexCoord2f(0.50f, 0.666f); glVertex3f(halfSize, -halfSize, -halfSize); // Нижний правый
    glTexCoord2f(0.25f, 0.666f); glVertex3f(-halfSize, -halfSize, -halfSize); // Нижний левый

    // Передняя грань (Front)
    glTexCoord2f(0.25f, 0.666f); glVertex3f(-halfSize, -halfSize, halfSize); // Верхний левый
    glTexCoord2f(0.50f, 0.666f); glVertex3f(halfSize, -halfSize, halfSize);  // Верхний правый
    glTexCoord2f(0.50f, 0.333f); glVertex3f(halfSize, halfSize, halfSize);   // Нижний правый
    glTexCoord2f(0.25f, 0.333f); glVertex3f(-halfSize, halfSize, halfSize);  // Нижний левый


    // Задняя грань (Back) — отзеркалена
    glTexCoord2f(0.75f, 0.666f); glVertex3f(halfSize, -halfSize, -halfSize);  // Верхний левый
    glTexCoord2f(1.0f, 0.666f); glVertex3f(-halfSize, -halfSize, -halfSize);  // Верхний правый
    glTexCoord2f(1.0f, 0.333f); glVertex3f(-halfSize, halfSize, -halfSize);   // Нижний правый
    glTexCoord2f(0.75f, 0.333f); glVertex3f(halfSize, halfSize, -halfSize);   // Нижний левый

    // Левая грань (Left)
    glTexCoord2f(0.0f, 0.666f); glVertex3f(-halfSize, -halfSize, -halfSize); // Верхний левый
    glTexCoord2f(0.25f, 0.666f); glVertex3f(-halfSize, -halfSize, halfSize);  // Верхний правый
    glTexCoord2f(0.25f, 0.333f); glVertex3f(-halfSize, halfSize, halfSize);   // Нижний правый
    glTexCoord2f(0.0f, 0.333f); glVertex3f(-halfSize, halfSize, -halfSize);   // Нижний левый

    // Правая грань (Right)
    glTexCoord2f(0.50f, 0.666f); glVertex3f(halfSize, -halfSize, halfSize);  // Верхний левый
    glTexCoord2f(0.75f, 0.666f); glVertex3f(halfSize, -halfSize, -halfSize); // Верхний правый
    glTexCoord2f(0.75f, 0.333f); glVertex3f(halfSize, halfSize, -halfSize);  // Нижний правый
    glTexCoord2f(0.50f, 0.333f); glVertex3f(halfSize, halfSize, halfSize);   // Нижний левый 
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

void calculateShadowMatrix(float shadowMatrix[16], const float ground[4], const float light[4]) {
    float dot = ground[0] * light[0] + ground[1] * light[1] + ground[2] * light[2] + ground[3] * light[3];

    shadowMatrix[0] = dot - light[0] * ground[0];
    shadowMatrix[1] = -light[0] * ground[1];
    shadowMatrix[2] = -light[0] * ground[2];
    shadowMatrix[3] = -light[0] * ground[3];

    shadowMatrix[4] = -light[1] * ground[0];
    shadowMatrix[5] = dot - light[1] * ground[1];
    shadowMatrix[6] = -light[1] * ground[2];
    shadowMatrix[7] = -light[1] * ground[3];

    shadowMatrix[8] = -light[2] * ground[0];
    shadowMatrix[9] = -light[2] * ground[1];
    shadowMatrix[10] = dot - light[2] * ground[2];
    shadowMatrix[11] = -light[2] * ground[3];

    shadowMatrix[12] = -light[3] * ground[0];
    shadowMatrix[13] = -light[3] * ground[1];
    shadowMatrix[14] = -light[3] * ground[2];
    shadowMatrix[15] = dot - light[3] * ground[3];
}

void drawShadows() {
    float shadowMatrix[16];
    float ground[4] = { 0.0f, 50.0f, 0.0f, 0.0f }; // Плоскость земли
    float lightPos[4] = { 1.0f, 50.0f, 1.0f, 0.0f }; // Источник света

    calculateShadowMatrix(shadowMatrix, ground, lightPos);

    glDisable(GL_LIGHTING); // Отключаем освещение для теней
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f); // Черный цвет с прозрачностью

    glPushMatrix();
    glMultMatrixf(shadowMatrix); // Применяем матрицу теней
    drawModel(); // Рисуем модель в тени
    glPopMatrix();

    glEnable(GL_LIGHTING); // Включаем освещение обратно
}


void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Камера
    float x = cos(pitch) * cos(yaw);
    float y = sin(pitch);
    float z = cos(pitch) * sin(yaw);
    gluLookAt(camX, camY, camZ, camX + x, camY + y, camZ + z, 0.0, 50.0, 0.0);

    drawTexturedCube(2000.0f, skyTexture); // Добавляем skybox перед отрисовкой объектов
    drawGround();
    drawShadows(); // Отрисовка теней
    drawModel();
    drawTree();

    glutSwapBuffers();
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 1.0f, 3000.0f);
    glMatrixMode(GL_MODELVIEW);
}

void mouseMotion(int x, int y) {
    if (firstMouse) {
        lastX = x;
        lastY = y;
        firstMouse = false;

        // Скрыть курсор и зафиксировать его в окне
        glutSetCursor(GLUT_CURSOR_NONE);
    }

    float xOffset = x - lastX;
    float yOffset = lastY - y;
    lastX = x;
    lastY = y;

    float sensitivity = 0.01f;
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    yaw += xOffset;
    pitch += yOffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    float cameraSpeed = 10.0f;

    if (key == 27) { // Код клавиши Esc
        exit(0); // Завершение программы
    }

    if (key == 'w') {
        camX += cos(pitch) * cos(yaw) * cameraSpeed;
        camY += sin(pitch) * cameraSpeed;
        camZ += cos(pitch) * sin(yaw) * cameraSpeed;
    }
    if (key == 's') {
        camX -= cos(pitch) * cos(yaw) * cameraSpeed;
        camY -= sin(pitch) * cameraSpeed;
        camZ -= cos(pitch) * sin(yaw) * cameraSpeed;
    }
    if (key == 'a') {
        camX -= sin(yaw) * cameraSpeed;
        camZ += cos(yaw) * cameraSpeed;
    }
    if (key == 'd') {
        camX += sin(yaw) * cameraSpeed;
        camZ -= cos(yaw) * cameraSpeed;
    }
    if (key == 'e') { // Пробел — подъем
        camY += cameraSpeed;
    }
    if (key == 'q') { // Shift — спуск
        camY -= cameraSpeed;
    }

    glutPostRedisplay(); // Перерисовываем сцену
}

int main(int argc, char** argv) {
    // Инициализация библиотеки GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1200, 900);
    glutCreateWindow("Forest road");

    // Инициализация GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    initOpenGL();

    // Устанавливаем функции обработки
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutPassiveMotionFunc(mouseMotion);  // Обработчик движения мыши
    glutKeyboardFunc(keyboard);  // Обработчик клавиш

    // Основной цикл отрисовки
    glutMainLoop();

    return 0;
}
