import React, { useState, useEffect } from 'react';

interface UserProfileProps {
  username: string;
  color: string;
  onSave: (newUsername: string) => void;
}

export const UserProfile: React.FC<UserProfileProps> = ({ username, color, onSave }) => {
  const [isEditing, setIsEditing] = useState(false);
  const [tempName, setTempName] = useState(username);

  useEffect(() => {
    setTempName(username);
  }, [username]);

  const handleSave = () => {
    if (tempName.trim()) {
      onSave(tempName.trim());
      setIsEditing(false);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') handleSave();
    if (e.key === 'Escape') {
      setTempName(username);
      setIsEditing(false);
    }
  };

  if (isEditing) {
    return (
      <div className="flex items-center space-x-2">
        <input
          type="text"
          value={tempName}
          onChange={(e) => setTempName(e.target.value)}
          onKeyDown={handleKeyDown}
          className="w-32 px-2 py-1 text-sm border border-gray-300 rounded focus:outline-none focus:ring-2 focus:ring-blue-500"
          autoFocus
        />
        <button
          onClick={handleSave}
          className="text-xs bg-blue-600 text-white px-2 py-1 rounded hover:bg-blue-700"
        >
          Save
        </button>
        <button
          onClick={() => {
            setTempName(username);
            setIsEditing(false);
          }}
          className="text-xs text-gray-500 hover:text-gray-700"
        >
          Cancel
        </button>
      </div>
    );
  }

  return (
    <div className="flex items-center space-x-2 group">
      <div
        className="w-8 h-8 rounded-full flex items-center justify-center text-white text-xs font-bold shadow-sm"
        style={{ background: color }}
      >
        {username.charAt(0).toUpperCase()}
      </div>
      <span className="text-sm font-medium text-gray-700">
        {username}
      </span>
      <button
        onClick={() => setIsEditing(true)}
        className="opacity-0 group-hover:opacity-100 text-xs text-blue-600 hover:underline ml-2 transition-opacity"
      >
        Edit
      </button>
    </div>
  );
};
