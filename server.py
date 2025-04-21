from flask import Flask, render_template, send_from_directory, send_file, g, request
import sqlite3
import os
import io
import zipfile
import urllib.parse

app = Flask(__name__, template_folder=os.getcwd())  # Use current dir for index.html

# --- Database connection ---
def get_db():
	db = getattr(g, '_database', None)
	if db is None:
		db = g._database = sqlite3.connect('example.db')
	return db

@app.teardown_appcontext
def close_connection(exception):
	db = getattr(g, '_database', None)
	if db is not None:
		db.close()

# --- Serve static CSS file ---
@app.route('/styles.css')
def serve_css():
	return send_from_directory(os.getcwd(), 'styles.css')

# --- Build the folder structure ---
def build_folder_tree():
	db = get_db()
	cursor = db.cursor()

	cursor.execute("SELECT DISTINCT ParentDirectory FROM Files WHERE ParentDirectory IS NOT NULL AND ParentDirectory != ''")
	folders = cursor.fetchall()

	cursor.execute("SELECT ID, FileName, FileType, ParentDirectory FROM Files WHERE ParentDirectory IS NULL OR ParentDirectory = ''")
	root_files = cursor.fetchall()

	folder_tree = {}
	for folder in folders:
		path_parts = folder[0].split("/")
		current_level = folder_tree
		for part in path_parts:
			if part not in current_level:
				current_level[part] = {}
			current_level = current_level[part]

		cursor.execute("SELECT ID, FileName, FileType, ParentDirectory FROM Files WHERE ParentDirectory=?", (folder[0],))
		files = cursor.fetchall()
		current_level["files"] = files

	return root_files, folder_tree

# --- Home route ---
@app.route("/")
def index():
	root_files, folder_tree = build_folder_tree()
	return render_template("index.html", root_files=root_files, folder_tree=folder_tree)

# --- Download individual file ---
@app.route("/download/<int:file_id>")
def download_file(file_id):
	db = get_db()
	cursor = db.cursor()
	cursor.execute("SELECT FileName, FileData FROM Files WHERE ID=?", (file_id,))
	file = cursor.fetchone()

	if file:
		file_name, file_data = file
		os.makedirs("temp", exist_ok=True)
		file_path = f"temp/{file_name}"

		with open(file_path, "wb") as f:
			f.write(file_data)

		return send_file(file_path, as_attachment=True)

	return "File not found!", 404

# --- Download full folder as ZIP ---
@app.route("/download_folder/<path:folder_path>")
def download_folder(folder_path):
	decoded_path = urllib.parse.unquote(folder_path)  # Decode URL-encoded slashes
	db = get_db()
	cursor = db.cursor()

	cursor.execute("SELECT ID, FileName, FileData FROM Files WHERE ParentDirectory=?", (decoded_path,))
	files = cursor.fetchall()

	if not files:
		return "No files in folder", 404

	zip_buffer = io.BytesIO()
	with zipfile.ZipFile(zip_buffer, 'w', zipfile.ZIP_DEFLATED) as zip_file:
		for file in files:
			file_name = file[1]
			file_data = file[2]
			zip_file.writestr(file_name, file_data)

	zip_buffer.seek(0)
	zip_name = decoded_path.split("/")[-1] or "folder"
	return send_file(zip_buffer, as_attachment=True, download_name=f"{zip_name}.zip", mimetype="application/zip")

# --- Update file location (drag & drop) ---
@app.route("/update_file_location", methods=["POST"])
def update_file_location():
	file_id = request.form.get('file_id')
	new_folder = request.form.get('new_folder')

	if not file_id or not new_folder:
		return "Missing file ID or folder path", 400

	db = get_db()
	cursor = db.cursor()
	cursor.execute("UPDATE Files SET ParentDirectory = ? WHERE ID = ?", (new_folder, file_id))
	db.commit()

	return "File location updated", 200

# --- Launch App ---
if __name__ == "__main__":
	app.run(debug=True)
